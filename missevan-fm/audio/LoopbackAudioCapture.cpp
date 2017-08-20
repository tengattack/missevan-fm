#include "StdAfx.h"
#include "LoopbackAudioCapture.h"

#include <MMDeviceAPI.h>
#include <AudioClient.h>

CLoopbackAudioCapture::CLoopbackAudioCapture(uint32 bufferLength)
	: CAudioCapture(bufferLength)
	, _ChatEndpoint(NULL)
	, _AudioClient(NULL)
	, _CaptureClient(NULL)
	, _ChatThread(NULL)
	, _ShutdownEvent(NULL)
	, _AudioSamplesReadyEvent(NULL)
{
}

CLoopbackAudioCapture::~CLoopbackAudioCapture()
{
	Shutdown();
}

bool CLoopbackAudioCapture::Initialize(AudioFormat *format)
{
	CAudioCapture::Initialize(format);

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
		mixFormat->wFormatTag = WAVE_FORMAT_PCM;
		mixFormat->nSamplesPerSec = _format.sampleRate;
		mixFormat->wBitsPerSample = _format.bits;
		mixFormat->nChannels = _format.channels;
		mixFormat->nBlockAlign = (mixFormat->nChannels * mixFormat->wBitsPerSample) / 8;
		mixFormat->nAvgBytesPerSec = mixFormat->nSamplesPerSec * mixFormat->nBlockAlign;
		mixFormat->cbSize = 0;
	}

	//
	//  Initialize the chat transport - Initialize WASAPI in event driven mode, associate the audio client with
	//  our samples ready event handle, retrieve a capture/render client for the transport, create the chat thread
	//  and start the audio engine.
	//
	GUID chatGuid;
	hr = CoCreateGuid(&chatGuid);
	if (FAILED(hr))
	{
		printf("Unable to create GUID.\n");
		return false;
	}

	hr = _AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_SESSIONFLAGS_DISPLAY_HIDE | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_LOOPBACK,
		_bufferLength * 10000 / mixFormat->nAvgBytesPerSec, // This parameter is of type REFERENCE_TIME and is expressed in 100-nanosecond units.
		0, mixFormat, &chatGuid);
	CoTaskMemFree(mixFormat);
	mixFormat = NULL;

	if (FAILED(hr))
	{
		printf("Unable to initialize audio client.\n");
		return false;
	}

	hr = _AudioClient->SetEventHandle(_AudioSamplesReadyEvent);
	if (FAILED(hr))
	{
		printf("Unable to set ready event.\n");
		return false;
	}

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
		return false;
	}

	return true;
}

void CLoopbackAudioCapture::Stop()
{
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

	SafeRelease(&_CaptureClient);
	SafeRelease(&_AudioClient);
}

DWORD CLoopbackAudioCapture::WasapiThread(LPVOID Context)
{
	bool stillPlaying = true;
	CLoopbackAudioCapture *pCapture = static_cast<CLoopbackAudioCapture *>(Context);
	HANDLE waitArray[2] = { pCapture->_ShutdownEvent, pCapture->_AudioSamplesReadyEvent };

	while (stillPlaying)
	{
		HRESULT hr;
		DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
		switch (waitResult)
		{
		case WAIT_OBJECT_0 + 0:
			stillPlaying = false;       // We're done, exit the loop.
			break;
		case WAIT_OBJECT_0 + 1: {
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
			hr = pCapture->_AudioClient->GetCurrentPadding(&framesAvailable);
			hr = pCapture->_CaptureClient->GetBuffer(&pData, &framesAvailable, &flags, NULL, NULL);

			if (pCapture->_callback)
			{
				AudioFormat *format = &pCapture->_format;
				pCapture->_callback(pData, framesAvailable * format->channels * format->bits / 8, pCapture, pCapture->_user_data);
			}

			hr = pCapture->_CaptureClient->ReleaseBuffer(framesAvailable);
			break;
		}
		}
	}
	return 0;
}