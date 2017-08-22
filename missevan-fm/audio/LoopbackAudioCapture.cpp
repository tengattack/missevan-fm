#include "StdAfx.h"
#include "LoopbackAudioCapture.h"

#include <MMDeviceAPI.h>
#include <AudioClient.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

FILE *fp = NULL;

CLoopbackAudioCapture::CLoopbackAudioCapture(uint32 bufferLength)
	: CAudioCapture(bufferLength)
	, _ChatEndpoint(NULL)
	, _AudioClient(NULL)
	, _CaptureClient(NULL)
	, _ChatThread(NULL)
	, _ShutdownEvent(NULL)
	, _AudioSamplesReadyEvent(NULL)
	, _EnableTransform(false)
{
	fp = fopen("D:\\CloudMusic\\t.pcm", "wb+");
	memset(&_waveFormat, 0, sizeof(_waveFormat));
}

CLoopbackAudioCapture::~CLoopbackAudioCapture()
{
	fclose(fp);
	Shutdown();
}

bool CLoopbackAudioCapture::Initialize(AudioFormat *format)
{
	CAudioCapture::_Initialize(format);

	IMMDeviceEnumerator *deviceEnumerator;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&deviceEnumerator));
	if (FAILED(hr))
	{
		printf("Unable to instantiate device enumerator\n");
		return false;
	}

	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &_ChatEndpoint);
	deviceEnumerator->Release();
	if (FAILED(hr))
	{
		printf("Unable to retrieve default endpoint\n");
		return false;
	}
	//
	//  Create our shutdown event - we want an auto reset event that starts in the not-signaled state.
	//
	_ShutdownEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_ShutdownEvent == NULL)
	{
		printf("Unable to create shutdown event.\n");
		return false;
	}

	_AudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_ShutdownEvent == NULL)
	{
		printf("Unable to create samples ready event.\n");
		return false;
	}

	_StartEvent.Create(false, false);

	return true;
}

void CLoopbackAudioCapture::Shutdown()
{
	Stop();

	if (_ShutdownEvent)
	{
		CloseHandle(_ShutdownEvent);
		_ShutdownEvent = NULL;
	}
	if (_AudioSamplesReadyEvent)
	{
		CloseHandle(_AudioSamplesReadyEvent);
		_AudioSamplesReadyEvent = NULL;
	}

	_StartEvent.Close();

	SafeRelease(&_ChatEndpoint);
}

bool CLoopbackAudioCapture::Start()
{
	WAVEFORMATEX *mixFormat = NULL;
	HRESULT hr = _ChatEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&_AudioClient));
	if (FAILED(hr))
	{
		printf("Unable to activate audio client.\n");
		return false;
	}
	hr = _AudioClient->GetMixFormat(&mixFormat);
	if (FAILED(hr))
	{
		printf("Unable to get mix format on audio client.\n");
		return false;
	}

	if (mixFormat->wFormatTag != WAVE_FORMAT_PCM)
	{
		if (!_Transform.Initialize(mixFormat, &_format))
		{
			printf("Unable to initialize audio transform.\n");
			return false;
		}

		_Transform.RegisterCallback(TransformProc, this);
		if (!_Transform.Start())
		{
			_Transform.Shutdown();
			printf("Unable to start audio transform.\n");
			return false;
		}

		_EnableTransform = true;
	}
	memcpy(&_waveFormat, mixFormat, sizeof(_waveFormat));

	//
	//  Initialize the chat transport - Initialize WASAPI in event driven mode, associate the audio client with
	//  our samples ready event handle, retrieve a capture/render client for the transport, create the chat thread
	//  and start the audio engine.
	//

	// REFTIMES_PER_SEC
	// AUDCLNT_SESSIONFLAGS_DISPLAY_HIDE | AUDCLNT_STREAMFLAGS_NOPERSIST | 
	// (uint64)_bufferLength * 10000000 / mixFormat->nAvgBytesPerSec
	hr = _AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_LOOPBACK,
		REFTIMES_PER_SEC, // This parameter is of type REFERENCE_TIME and is expressed in 100-nanosecond units.
		0, mixFormat, NULL);
	CoTaskMemFree(mixFormat);
	mixFormat = NULL;

	if (FAILED(hr))
	{
		printf("Unable to initialize audio client.\n");
		return false;
	}

	/*hr = _AudioClient->SetEventHandle(_AudioSamplesReadyEvent);
	if (FAILED(hr))
	{
		printf("Unable to set ready event.\n");
		return false;
	}*/

	hr = _AudioClient->GetService(IID_PPV_ARGS(&_CaptureClient));
	if (FAILED(hr))
	{
		printf("Unable to get Capture/Render client.\n");
		return false;
	}

	//
	//  Now create the thread which is going to drive the "Chat".
	//
	_ChatThread = CreateThread(NULL, 0, WasapiThread, this, 0, NULL);
	if (_ChatThread == NULL)
	{
		printf("Unable to create transport thread.\n");
		return false;
	}

	//
	//  We're ready to go, start the chat!
	//
	hr = _AudioClient->Start();
	if (FAILED(hr))
	{
		printf("Unable to start chat client.\n");
		_StartEvent.Set();
		return false;
	}

	_StartEvent.Set();
	return true;
}

void CLoopbackAudioCapture::Stop()
{
	_AudioClient->Stop();

	if (_ShutdownEvent)
	{
		SetEvent(_ShutdownEvent);
	}
	if (_ChatThread)
	{
		WaitForSingleObject(_ChatThread, INFINITE);

		CloseHandle(_ChatThread);
		_ChatThread = NULL;
	}
	if (_EnableTransform) {
		_Transform.Stop();
		_Transform.Shutdown();
		_EnableTransform = false;
	}

	SafeRelease(&_CaptureClient);
	SafeRelease(&_AudioClient);

	memset(&_waveFormat, 0, sizeof(_waveFormat));
}

DWORD CLoopbackAudioCapture::WasapiThread(LPVOID Context)
{
	bool stillPlaying = true;
	CLoopbackAudioCapture *pCapture = static_cast<CLoopbackAudioCapture *>(Context);
	HANDLE waitArray[2] = { pCapture->_ShutdownEvent /*, pCapture->_AudioSamplesReadyEvent */ };

	pCapture->_StartEvent.Wait();

	WAVEFORMATEX *pwfx = &pCapture->_waveFormat;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	UINT32 packetLength = 0;
	UINT32 bufferFrameCount;
	// Get the size of the allocated buffer.
	HRESULT hr = pCapture->_AudioClient->GetBufferSize(&bufferFrameCount);
	if (FAILED(hr)) {
		goto exit_l;
	}

	// Calculate the actual duration of the allocated buffer.
	hnsActualDuration = (double)REFTIMES_PER_SEC *
		bufferFrameCount / pwfx->nSamplesPerSec;

	while (stillPlaying)
	{
		HRESULT hr;
		DWORD waitResult = WaitForSingleObject(pCapture->_ShutdownEvent, hnsActualDuration / REFTIMES_PER_MILLISEC / 2);
		switch (waitResult)
		{
		case WAIT_OBJECT_0 + 0:
			stillPlaying = false;       // We're done, exit the loop.
			break;
		case WAIT_TIMEOUT: {

			hr = pCapture->_CaptureClient->GetNextPacketSize(&packetLength);
			if (FAILED(hr)) {
				goto exit_l;
			}

			while (packetLength != 0)
			{
				//
				//  Either stream silence to the audio client or ignore the audio samples.
				//
				//  Note that we don't check for errors here.  This is because 
				//      (a) there's no way of reporting the failure
				//      (b) once the streaming engine has started there's really no way for it to fail.
				//
				BYTE *pData;
				UINT32 framesAvailable;
				DWORD flags;

				// hr = pCapture->_AudioClient->GetCurrentPadding(&framesAvailable);
				hr = pCapture->_CaptureClient->GetBuffer(&pData, &framesAvailable, &flags, NULL, NULL);
				if (FAILED(hr)) {
					goto exit_l;
				}

				if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					// TODO: silent
				}
				else if (pCapture->_EnableTransform)
				{
					pCapture->_Transform.Encode(pData, framesAvailable * pwfx->nChannels * pwfx->wBitsPerSample / 8);
				}
				else if (pCapture->_callback)
				{
					AudioFormat *format = &pCapture->_format;
					pCapture->_callback(pData, framesAvailable * format->channels * format->bits / 8, pCapture->_user_data);
				}

				hr = pCapture->_CaptureClient->ReleaseBuffer(framesAvailable);
			}
			break;
		}
		}
	}

exit_l:
	return 0;
}

void CLoopbackAudioCapture::TransformProc(uint8 *data, ulong length, void *user_data)
{
	CLoopbackAudioCapture *pCapture = (CLoopbackAudioCapture *)user_data;
	if (pCapture->_callback)
	{
		pCapture->_callback(data, length, pCapture->_user_data);
	}
	fwrite(data, sizeof(uint8), length, fp);
}