#include "StdAfx.h"
#include "AudioTransform.h"

#include <common/Buffer.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <atlbase.h>
#include <wmcodecdsp.h>

CAudioTransform::CAudioTransform()
	: CCallbackAble()
	, _pTransform(NULL)
{
}


CAudioTransform::~CAudioTransform()
{
}

bool CAudioTransform::Init()
{
	HFG(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
	return true;
exit_l:
	return false;
}

void CAudioTransform::Cleanup()
{
	MFShutdown();
}

bool CAudioTransform::Initialize(WAVEFORMATEX *from, AudioFormat *to)
{
	CComPtr<IUnknown> spTransformUnk;
	CComPtr<IWMResamplerProps> spResamplerProps;
	CComPtr<IMFMediaType> pMediaType;
	CComPtr<IMFMediaType> pMediaOutputType;
	WAVEFORMATEX wfx;

	HFG(CoCreateInstance(CLSID_CResamplerMediaObject, NULL, CLSCTX_INPROC_SERVER,
			IID_IUnknown, (void**)&spTransformUnk));

	HFG(spTransformUnk->QueryInterface(IID_PPV_ARGS(&_pTransform)));

	HFG(spTransformUnk->QueryInterface(IID_PPV_ARGS(&spResamplerProps)));
	HFG(spResamplerProps->SetHalfFilterLength(60)); // < best conversion quality

	// set input PCM format parameters to fmt
	HFG(MFCreateMediaType(&pMediaType));
	HFG(MFInitMediaTypeFromWaveFormatEx(pMediaType, from, sizeof(WAVEFORMATEX) + from->cbSize));
	HFG(_pTransform->SetInputType(0, pMediaType, 0));

	// set output parameters
	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nSamplesPerSec = to->sampleRate;
	wfx.wBitsPerSample = to->bits;
	wfx.nChannels = to->channels;
	wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize = 0;

	HFG(MFCreateMediaType(&pMediaOutputType));
	HFG(MFInitMediaTypeFromWaveFormatEx(pMediaOutputType, &wfx, sizeof(WAVEFORMATEX)));
	HFG(_pTransform->SetOutputType(0, pMediaOutputType, 0));

	memcpy(&_form, from, sizeof(_form));
	memcpy(&_to, to, sizeof(_to));

	return true;

exit_l:
	SafeRelease(&_pTransform);
	return false;
}

bool CAudioTransform::Start()
{
	HFG(_pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL));
	HFG(_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL));
	HFG(_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL));

	return true;

exit_l:
	return false;
}

void CAudioTransform::Stop()
{
	HFG(_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL));
	HFG(_pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL));
	HFG(_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, NULL));
exit_l:
	return;
}

void CAudioTransform::Shutdown()
{
	if (_pTransform) {
		Stop();
		SafeRelease(&_pTransform);
	}
	_callback = NULL;
	_user_data = NULL;
}

bool CAudioTransform::Encode(uint8 *data, ulong length)
{
	CBuffer buf;
	CComPtr<IMFMediaBuffer> pBuffer = NULL;
	CComPtr<IMFSample> pSample = NULL;
	CComPtr<IMFMediaBuffer> pBufferOut = NULL;
	MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
	BYTE  *pByteBufferTo = NULL;
	DWORD dwStatus;
	DWORD cbBytes = 0;
	BYTE  *pByteBuffer = NULL;
	HRESULT hr;

	HFG(MFCreateMemoryBuffer(length, &pBuffer));
	HFG(pBuffer->Lock(&pByteBufferTo, NULL, NULL));

	memcpy(pByteBufferTo, data, length);
	pBuffer->Unlock();
	pByteBufferTo = NULL;

	HFG(pBuffer->SetCurrentLength(length));

	HFG(MFCreateSample(&pSample));
	HFG(pSample->AddBuffer(pBuffer));

	HFG(_pTransform->ProcessInput(0, pSample, 0));

	HFG(MFCreateSample(&(outputDataBuffer.pSample)));
	HFG(MFCreateMemoryBuffer(length * 2, &pBufferOut));
	HFG(outputDataBuffer.pSample->AddBuffer(pBufferOut));

	do {
		outputDataBuffer.dwStreamID = 0;
		outputDataBuffer.dwStatus = 0;
		outputDataBuffer.pEvents = NULL;

		hr = _pTransform->ProcessOutput(0, 1, &outputDataBuffer, &dwStatus);
		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
			// conversion end
			break;
		}

		// output PCM data is set in outputDataBuffer.pSample;
		IMFSample *pSample_ = outputDataBuffer.pSample;

		CComPtr<IMFMediaBuffer> spBuffer;
		pSample_->ConvertToContiguousBuffer(&spBuffer);
		spBuffer->GetCurrentLength(&cbBytes);

		hr = spBuffer->Lock(&pByteBuffer, NULL, NULL);

		buf.Write(pByteBuffer, cbBytes);

		spBuffer->Unlock();
	} while (true);

	SafeRelease(&outputDataBuffer.pSample);

	if (_callback) {
		_callback(buf.GetBuffer(), buf.GetBufferLen(), buf.GetBufferLen() / (_to.bits / 8), _user_data);
	}

	return true;

exit_l:
	return false;
}