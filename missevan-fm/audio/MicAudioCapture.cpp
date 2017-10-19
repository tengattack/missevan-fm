#include "StdAfx.h"
#include "MicAudioCapture.h"

#include <base/logging.h>
#include <boost/format.hpp>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <FunctionDiscoveryKeys_devpkey.h>
#include <avrt.h>

bool DisableMMCSS = false;

CMicAudioCapture::CMicAudioCapture(uint32 bufferLength)
	: CAudioCapture(bufferLength)
	, _Endpoint(NULL)
	, _AudioClient(NULL)
	, _CaptureClient(NULL)
	, _CaptureThread(NULL)
	, _ShutdownEvent(NULL)
	, _AudioSamplesReadyEvent(NULL)
	, _CurrentCaptureIndex(0)
	, _EnableTransform(false)
	, _stopped(false)
{
	memset(&_waveFormat, 0, sizeof(_waveFormat));
	//
	//  configured latency in case we'll need it for a stream switch later.
	//
	_EngineLatencyInMS = 500;
}

CMicAudioCapture::~CMicAudioCapture()
{
	Shutdown();
}


//
//  Initialize WASAPI in event driven mode, associate the audio client with our samples ready event handle, retrieve 
//  a capture client for the transport, create the capture thread and start the audio engine.
//
bool CMicAudioCapture::InitializeAudioEngine()
{
	//
	//  Retrieve the buffer size for the audio client.
	//
	HRESULT hr = _AudioClient->GetBufferSize(&_BufferSize);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to get audio client buffer: " << boost::format("0x%08x") % hr;
		return false;
	}

	hr = _AudioClient->SetEventHandle(_AudioSamplesReadyEvent);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to set ready event: " << boost::format("0x%08x") % hr;
		return false;
	}

	hr = _AudioClient->GetService(IID_PPV_ARGS(&_CaptureClient));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to get new capture client: " << boost::format("0x%08x") % hr;
		return false;
	}

	return true;
}

//
//  Initialize the capturer.
//
bool CMicAudioCapture::Initialize(AudioFormat *format)
{
	CAudioCapture::Initialize(format);

	//
	//  Create our shutdown and samples ready events- we want auto reset events that start in the not-signaled state.
	//
	_ShutdownEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_ShutdownEvent == NULL)
	{
		PLOG(ERROR) << "Unable to create shutdown event";
		return false;
	}

	_AudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_AudioSamplesReadyEvent == NULL)
	{
		PLOG(ERROR) << "Unable to create samples ready event";
		return false;
	}

	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_DeviceEnumerator));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to instantiate device enumerator: " << boost::format("0x%08x") % hr;
		return false;
	}

	hr = _DeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &_Endpoint);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to retrieve default endpoint: " << boost::format("0x%08x") % hr;
		return false;
	}

	return true;
}

//
//  Shut down the capture code and free all the resources.
//
void CMicAudioCapture::Shutdown()
{
	if (_CaptureThread)
	{
		SetEvent(_ShutdownEvent);
		WaitForSingleObject(_CaptureThread, INFINITE);
		CloseHandle(_CaptureThread);
		_CaptureThread = NULL;
	}

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

	SafeRelease(&_Endpoint);
	SafeRelease(&_AudioClient);
	SafeRelease(&_CaptureClient);

	TerminateStreamSwitch();
}

//
//  Start capturing...
//
bool CMicAudioCapture::Start()
{
	HRESULT hr;
	WAVEFORMATEX *mixFormat = NULL;

	if (_EnableTransform) {
		// disbale transformer for previous device
		_Transform.Shutdown();
		_EnableTransform = false;
	}

	//
	//  Now activate an IAudioClient object on our preferred endpoint and retrieve the mix format for that endpoint.
	//
	hr = _Endpoint->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&_AudioClient));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to activate audio client: " << boost::format("0x%08x") % hr;
		return false;
	}

	hr = _AudioClient->GetMixFormat(&mixFormat);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to get mix format on audio client: " << boost::format("0x%08x") % hr;
		return false;
	}

	if (mixFormat->wFormatTag != WAVE_FORMAT_PCM
		|| mixFormat->nSamplesPerSec != _format.sampleRate
		|| mixFormat->nChannels != _format.channels
		|| mixFormat->wBitsPerSample != _format.bits)
	{
		WAVEFORMATEXTENSIBLE *cloestWaveFormat = NULL;
		_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		_waveFormat.nSamplesPerSec = _format.sampleRate;
		_waveFormat.nChannels = _format.channels;
		_waveFormat.wBitsPerSample = _format.bits;
		_waveFormat.nBlockAlign = (_waveFormat.nChannels * _waveFormat.wBitsPerSample) / 8;
		_waveFormat.nAvgBytesPerSec = _waveFormat.nSamplesPerSec * _waveFormat.nBlockAlign;
		_waveFormat.cbSize = 0;

		// try initialize for specify format
		hr = _AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &_waveFormat, (WAVEFORMATEX **)&cloestWaveFormat);

		if (hr == S_OK) {
			// the format is supported
			memcpy(mixFormat, &_waveFormat, sizeof(_waveFormat));
		} else {
			memcpy(&_waveFormat, mixFormat, sizeof(_waveFormat));

			if (!_Transform.Initialize(mixFormat, &_format))
			{
				PLOG(ERROR) << "Unable to initialize audio transformer.";
				return false;
			}

			_Transform.RegisterCallback(TransformProc, this);
			if (!_Transform.Start())
			{
				_Transform.Shutdown();
				PLOG(ERROR) << "Unable to start audio transformer.";
				return false;
			}

			LOG(INFO) << "Enable audio transformer (CMicAudioCapture)";
			_EnableTransform = true;
		}

		if (cloestWaveFormat) {
			CoTaskMemFree(cloestWaveFormat);
		}
	} else {
		memcpy(&_waveFormat, mixFormat, sizeof(_waveFormat));
	}

	hr = _AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, _EngineLatencyInMS * 10000, 0, mixFormat, NULL);

	_FrameSize = (mixFormat->wBitsPerSample / 8) * mixFormat->nChannels;
	CoTaskMemFree(mixFormat);
	mixFormat = NULL;

	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to initialize audio client: " << boost::format("0x%08x") % hr;
		return false;
	}

	if (!InitializeAudioEngine())
	{
		return false;
	}

	if (!InitializeStreamSwitch(_AudioClient, eCapture, eCommunications))
	{
		return false;
	}

	if (_CaptureThread == NULL)
	{
		//
		//  Now create the thread which is going to drive the capture.
		//
		_CaptureThread = CreateThread(NULL, 0, WASAPICaptureThread, this, 0, NULL);
		if (_CaptureThread == NULL)
		{
			PLOG(ERROR) << "Unable to create transport thread";
			return false;
		}
	}

	//
	//  We're ready to go, start capturing!
	//
	hr = _AudioClient->Start();
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to start capture client: " << boost::format("0x%08x") % hr;
		return false;
	}

	return true;
}

//
//  Stop the capturer.
//
void CMicAudioCapture::Stop()
{
	//
	//  Tell the capture thread to shut down, wait for the thread to complete then clean up all the stuff we 
	//  allocated in Start().
	//
	if (_ShutdownEvent)
	{
		SetEvent(_ShutdownEvent);
	}

	if (_AudioClient)
	{
		HRESULT hr;
		hr = _AudioClient->Stop();
		if (FAILED(hr))
		{
			PLOG(ERROR) << "Unable to stop audio client: " << boost::format("0x%08x") % hr;
		}
	}

	if (_CaptureThread)
	{
		WaitForSingleObject(_CaptureThread, INFINITE);

		CloseHandle(_CaptureThread);
		_CaptureThread = NULL;
	}

	if (_EnableTransform)
	{
		_Transform.Stop();
		_Transform.Shutdown();
		_EnableTransform = false;
	}

	SafeRelease(&_AudioClient);
	SafeRelease(&_CaptureClient);

	memset(&_waveFormat, 0, sizeof(_waveFormat));
}

bool CMicAudioCapture::HandleStreamSwitchEvent()
{
	if (_AudioClient)
	{
		_AudioClient->Stop();
	}
	SafeRelease(&_Endpoint);
	SafeRelease(&_AudioClient);
	SafeRelease(&_CaptureClient);

	HRESULT hr = _DeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &_Endpoint);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to retrieve default endpoint: " << boost::format("0x%08x") % hr;
		return false;
	}

	return Start();
}

//
//  Capture thread - processes samples from the audio engine
//
DWORD CMicAudioCapture::WASAPICaptureThread(LPVOID Context)
{
	CMicAudioCapture *capturer = static_cast<CMicAudioCapture *>(Context);
	return capturer->DoCaptureThread();
}

DWORD CMicAudioCapture::DoCaptureThread()
{
	bool stillPlaying = true;
	HANDLE waitArray[3] = { _ShutdownEvent, _StreamSwitchEvent, _AudioSamplesReadyEvent };
	HANDLE mmcssHandle = NULL;
	DWORD mmcssTaskIndex = 0;
	WAVEFORMATEX *pwfx = &_waveFormat;

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to initialize COM in render thread: " << boost::format("0x%08x") % hr;
		return hr;
	}

	if (!DisableMMCSS)
	{
		mmcssHandle = AvSetMmThreadCharacteristics(L"Audio", &mmcssTaskIndex);
		if (mmcssHandle == NULL)
		{
			PLOG(ERROR) << "Unable to enable MMCSS on capture thread";
		}
	}

	while (stillPlaying)
	{
		HRESULT hr;
		DWORD waitResult = WaitForMultipleObjects(3, waitArray, FALSE, INFINITE);
		switch (waitResult)
		{
		case WAIT_OBJECT_0 + 0:     // _ShutdownEvent
			stillPlaying = false;       // We're done, exit the loop.
			break;
		case WAIT_OBJECT_0 + 1:     // _StreamSwitchEvent
									//
									//  We need to stop the capturer, tear down the _AudioClient and _CaptureClient objects and re-create them on the new.
									//  endpoint if possible.  If this fails, abort the thread.
									//
			if (!HandleStreamSwitchEvent())
			{
				// stillPlaying = false;
			}
			else
			{
				_InStreamSwitch = false;
			}
			break;
		case WAIT_OBJECT_0 + 2:     // _AudioSamplesReadyEvent
									//
									//  We need to retrieve the next buffer of samples from the audio capturer.
									//
			if (_CaptureClient == NULL)
			{
				break;
			}

			BYTE *pData;
			UINT32 framesAvailable;
			DWORD  flags;

			//
			//  Find out how much capture data is available.  We need to make sure we don't run over the length
			//  of our capture buffer.  We'll discard any samples that don't fit in the buffer.
			//
			hr = _CaptureClient->GetBuffer(&pData, &framesAvailable, &flags, NULL, NULL);
			if (SUCCEEDED(hr))
			{
				if (hr != S_OK) {
					// may AUDCLNT_S_BUFFER_EMPTY
					break;
				}
				if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					if (_callback) {
						uint32 length = framesAvailable * _format.channels * _format.bits / 8;
						_callback(kCaptureData, NULL, length, _user_data);
					}
				}
				else if (_EnableTransform)
				{
					_Transform.Encode(pData, framesAvailable * pwfx->nChannels * pwfx->wBitsPerSample / 8);
				}
				else if (_callback)
				{
					_callback(kCaptureData, pData, framesAvailable * _format.channels * _format.bits / 8, _user_data);
				}

				hr = _CaptureClient->ReleaseBuffer(framesAvailable);
				if (FAILED(hr))
				{
					PLOG(ERROR) << "Unable to release capture buffer: " << boost::format("0x%08x") % hr;
				}
			} else {
				PLOG(ERROR) << "Unable to get capture buffer: " << boost::format("0x%08x") % hr;
			}
			break;
		}
	}
	if (!DisableMMCSS)
	{
		AvRevertMmThreadCharacteristics(mmcssHandle);
	}

	CoUninitialize();
	return 0;
}

void CMicAudioCapture::TransformProc(uint8 *data, ulong length, ulong samples, void *user_data)
{
	CMicAudioCapture *pCapture = (CMicAudioCapture *)user_data;
	if (pCapture->_callback)
	{
		pCapture->_callback(kCaptureData, data, length, pCapture->_user_data);
	}
}
