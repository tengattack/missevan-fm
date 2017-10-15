#include "StdAfx.h"
#include "MicAudioCapture.h"

#include <base/logging.h>
#include <boost/format.hpp>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <FunctionDiscoveryKeys_devpkey.h>
#include <avrt.h>

bool DisableMMCSS = false;

class CMMNotificationClient : public IMMNotificationClient
{
	LONG _cRef;
	IMMDeviceEnumerator *_pEnumerator;

	// Private function to print device-friendly name
	HRESULT _PrintDeviceName(LPCWSTR  pwstrId);

public:
	CMMNotificationClient() :
		_cRef(1),
		_pEnumerator(NULL)
	{
	}

	~CMMNotificationClient()
	{
		SafeRelease(&_pEnumerator);
	}

	// IUnknown methods -- AddRef, Release, and QueryInterface

	ULONG STDMETHODCALLTYPE AddRef()
	{
		return InterlockedIncrement(&_cRef);
	}

	ULONG STDMETHODCALLTYPE Release()
	{
		ULONG ulRef = InterlockedDecrement(&_cRef);
		if (0 == ulRef)
		{
			delete this;
		}
		return ulRef;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(
		REFIID riid, VOID **ppvInterface)
	{
		if (IID_IUnknown == riid)
		{
			AddRef();
			*ppvInterface = (IUnknown*)this;
		}
		else if (__uuidof(IMMNotificationClient) == riid)
		{
			AddRef();
			*ppvInterface = (IMMNotificationClient*)this;
		}
		else
		{
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}
		return S_OK;
	}

	// Callback methods for device-event notifications.

	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
		EDataFlow flow, ERole role,
		LPCWSTR pwstrDeviceId)
	{
		char  *pszFlow = "?????";
		char  *pszRole = "?????";

		_PrintDeviceName(pwstrDeviceId);

		switch (flow)
		{
		case eRender:
			pszFlow = "eRender";
			break;
		case eCapture:
			pszFlow = "eCapture";
			break;
		}

		switch (role)
		{
		case eConsole:
			pszRole = "eConsole";
			break;
		case eMultimedia:
			pszRole = "eMultimedia";
			break;
		case eCommunications:
			pszRole = "eCommunications";
			break;
		}

		printf("  -->New default device: flow = %s, role = %s\n",
			pszFlow, pszRole);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId)
	{
		_PrintDeviceName(pwstrDeviceId);

		printf("  -->Added device\n");
		return S_OK;
	};

	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId)
	{
		_PrintDeviceName(pwstrDeviceId);

		printf("  -->Removed device\n");
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(
		LPCWSTR pwstrDeviceId,
		DWORD dwNewState)
	{
		char  *pszState = "?????";

		_PrintDeviceName(pwstrDeviceId);

		switch (dwNewState)
		{
		case DEVICE_STATE_ACTIVE:
			pszState = "ACTIVE";
			break;
		case DEVICE_STATE_DISABLED:
			pszState = "DISABLED";
			break;
		case DEVICE_STATE_NOTPRESENT:
			pszState = "NOTPRESENT";
			break;
		case DEVICE_STATE_UNPLUGGED:
			pszState = "UNPLUGGED";
			break;
		}

		printf("  -->New device state is DEVICE_STATE_%s (0x%8.8x)\n",
			pszState, dwNewState);

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
		LPCWSTR pwstrDeviceId,
		const PROPERTYKEY key)
	{
		_PrintDeviceName(pwstrDeviceId);

		printf("  -->Changed device property "
			"{%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x}#%d\n",
			key.fmtid.Data1, key.fmtid.Data2, key.fmtid.Data3,
			key.fmtid.Data4[0], key.fmtid.Data4[1],
			key.fmtid.Data4[2], key.fmtid.Data4[3],
			key.fmtid.Data4[4], key.fmtid.Data4[5],
			key.fmtid.Data4[6], key.fmtid.Data4[7],
			key.pid);
		return S_OK;
	}
};

// Given an endpoint ID string, print the friendly device name.
HRESULT CMMNotificationClient::_PrintDeviceName(LPCWSTR pwstrId)
{
	HRESULT hr = S_OK;
	IMMDevice *pDevice = NULL;
	IPropertyStore *pProps = NULL;
	PROPVARIANT varString;

	PropVariantInit(&varString);

	if (_pEnumerator == NULL)
	{
		// Get enumerator for audio endpoint devices.
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
			NULL, CLSCTX_INPROC_SERVER,
			__uuidof(IMMDeviceEnumerator),
			(void**)&_pEnumerator);
	}
	if (hr == S_OK)
	{
		hr = _pEnumerator->GetDevice(pwstrId, &pDevice);
	}
	if (hr == S_OK)
	{
		hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
	}
	if (hr == S_OK)
	{
		// Get the endpoint device's friendly-name property.
		hr = pProps->GetValue(PKEY_Device_FriendlyName, &varString);
	}
	wprintf(L"----------------------\nDevice name: \"%s\"\n"
		L"  Endpoint ID string: \"%s\"\n",
		(hr == S_OK) ? varString.pwszVal : L"null device",
		(pwstrId != NULL) ? pwstrId : L"null ID");

	PropVariantClear(&varString);

	SafeRelease(&pProps);
	SafeRelease(&pDevice);
	return hr;
}

CMicAudioCapture::CMicAudioCapture(uint32 bufferLength)
	: CAudioCapture(bufferLength)
	, _RefCount(1)
	, _Endpoint(NULL)
	, _AudioClient(NULL)
	, _CaptureClient(NULL)
	, _CaptureThread(NULL)
	, _ShutdownEvent(NULL)
	, _AudioSamplesReadyEvent(NULL)
	, _CurrentCaptureIndex(0)
	, _EnableStreamSwitch(false)
	, _EndpointRole(eCommunications)
	, _StreamSwitchEvent(NULL)
	, _StreamSwitchCompleteEvent(NULL)
	, _AudioSessionControl(NULL)
	, _DeviceEnumerator(NULL)
	, _InStreamSwitch(false)
	, _EnableTransform(false)
	, _stopped(false)
{
	memset(&_waveFormat, 0, sizeof(_waveFormat));
	_notifyClient = new CMMNotificationClient();
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
	CAudioCapture::_Initialize(format);

	_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	_waveFormat.nChannels = format->channels;
	_waveFormat.nSamplesPerSec = format->sampleRate;
	_waveFormat.wBitsPerSample = format->bits;
	_waveFormat.nBlockAlign = (_waveFormat.wBitsPerSample / 8) * _waveFormat.nChannels;
	_waveFormat.nAvgBytesPerSec = _waveFormat.nSamplesPerSec * _waveFormat.nBlockAlign;
	_waveFormat.cbSize = 0;

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

	//
	//  Create our stream switch event- we want auto reset events that start in the not-signaled state.
	//  Note that we create this event even if we're not going to stream switch - that's because the event is used
	//  in the main loop of the capturer and thus it has to be set.
	//
	_StreamSwitchEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_StreamSwitchEvent == NULL)
	{
		PLOG(ERROR) << "Unable to create stream switch event";
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

	//
	//  Now activate an IAudioClient object on our preferred endpoint and retrieve the mix format for that endpoint.
	//
	hr = _Endpoint->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&_AudioClient));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to activate audio client: " << boost::format("0x%08x") % hr;
		return false;
	}

	WAVEFORMATEX *mixFormat = NULL;
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

			_EnableTransform = true;
		}

		if (cloestWaveFormat) {
			CoTaskMemFree(cloestWaveFormat);
		}
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

	if (_EnableStreamSwitch)
	{
		if (!InitializeStreamSwitch())
		{
			return false;
		}
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
	if (_StreamSwitchEvent)
	{
		CloseHandle(_StreamSwitchEvent);
		_StreamSwitchEvent = NULL;
	}

	SafeRelease(&_Endpoint);
	SafeRelease(&_AudioClient);
	SafeRelease(&_CaptureClient);

	if (_EnableStreamSwitch)
	{
		TerminateStreamSwitch();
	}
}


//
//  Start capturing...
//
bool CMicAudioCapture::Start()
{
	HRESULT hr;

	//
	//  Now create the thread which is going to drive the capture.
	//
	_CaptureThread = CreateThread(NULL, 0, WASAPICaptureThread, this, 0, NULL);
	if (_CaptureThread == NULL)
	{
		PLOG(ERROR) << "Unable to create transport thread";
		return false;
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
	HRESULT hr;

	//
	//  Tell the capture thread to shut down, wait for the thread to complete then clean up all the stuff we 
	//  allocated in Start().
	//
	if (_ShutdownEvent)
	{
		SetEvent(_ShutdownEvent);
	}

	hr = _AudioClient->Stop();
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to stop audio client: " << boost::format("0x%08x") % hr;
	}

	if (_CaptureThread)
	{
		WaitForSingleObject(_CaptureThread, INFINITE);

		CloseHandle(_CaptureThread);
		_CaptureThread = NULL;
	}
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
			/* if (!HandleStreamSwitchEvent())
			{
				stillPlaying = false;
			} */
			break;
		case WAIT_OBJECT_0 + 2:     // _AudioSamplesReadyEvent
									//
									//  We need to retrieve the next buffer of samples from the audio capturer.
									//
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
				if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					if (_callback) {
						uint32 length = framesAvailable * _format.channels * _format.bits / 8;
						_callback(NULL, length, _user_data);
					}
				}
				else if (_EnableTransform)
				{
					_Transform.Encode(pData, framesAvailable * pwfx->nChannels * pwfx->wBitsPerSample / 8);
				}
				else if (_callback)
				{
					_callback(pData, framesAvailable * _format.channels * _format.bits / 8, _user_data);
				}

				/* UINT32 framesToCopy = std::min(framesAvailable, static_cast<UINT32>((_CaptureBufferSize - _CurrentCaptureIndex) / _FrameSize));
				if (framesToCopy != 0)
				{
					//
					//  The flags on capture tell us information about the data.
					//
					//  We only really care about the silent flag since we want to put frames of silence into the buffer
					//  when we receive silence.  We rely on the fact that a logical bit 0 is silence for both float and int formats.
					//
					if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
					{
						//
						//  Fill 0s from the capture buffer to the output buffer.
						//
						ZeroMemory(&_CaptureBuffer[_CurrentCaptureIndex], framesToCopy*_FrameSize);
					}
					else
					{
						//
						//  Copy data from the audio engine buffer to the output buffer.
						//
						CopyMemory(&_CaptureBuffer[_CurrentCaptureIndex], pData, framesToCopy*_FrameSize);
					}
					//
					//  Bump the capture buffer pointer.
					//
					_CurrentCaptureIndex += framesToCopy*_FrameSize;
				} */
				hr = _CaptureClient->ReleaseBuffer(framesAvailable);
				if (FAILED(hr))
				{
					PLOG(ERROR) << "Unable to release capture buffer: " << boost::format("0x%08x") % hr;
				}
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


//
//  Initialize the stream switch logic.
//
bool CMicAudioCapture::InitializeStreamSwitch()
{
	HRESULT hr = _AudioClient->GetService(IID_PPV_ARGS(&_AudioSessionControl));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to retrieve session control: " << boost::format("0x%08x") % hr;
		return false;
	}

	//
	//  Create the stream switch complete event- we want a manual reset event that starts in the not-signaled state.
	//
	_StreamSwitchCompleteEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_INITIAL_SET | CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_StreamSwitchCompleteEvent == NULL)
	{
		PLOG(ERROR) << "Unable to create stream switch event";
		return false;
	}
	//
	//  Register for session and endpoint change notifications.  
	//
	//  A stream switch is initiated when we receive a session disconnect notification or we receive a default device changed notification.
	//
	/* hr = _AudioSessionControl->RegisterAudioSessionNotification(this);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to register for session notifications: " << boost::format("0x%08x") % hr;
		return false;
	}

	hr = _DeviceEnumerator->RegisterEndpointNotificationCallback(this);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to register for endpoint notifications: " << boost::format("0x%08x") % hr;
		return false;
	} */

	return true;
}

void CMicAudioCapture::TerminateStreamSwitch()
{
	/* HRESULT hr = _AudioSessionControl->UnregisterAudioSessionNotification(this);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to unregister for session notifications: " << boost::format("0x%08x") % hr;
	}

	_DeviceEnumerator->UnregisterEndpointNotificationCallback(this);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to unregister for endpoint notifications: " << boost::format("0x%08x") % hr;
	} */

	if (_StreamSwitchCompleteEvent)
	{
		CloseHandle(_StreamSwitchCompleteEvent);
		_StreamSwitchCompleteEvent = NULL;
	}

	SafeRelease(&_AudioSessionControl);
	SafeRelease(&_DeviceEnumerator);
}

void CMicAudioCapture::TransformProc(uint8 *data, ulong length, ulong samples, void *user_data)
{
	CMicAudioCapture *pCapture = (CMicAudioCapture *)user_data;
	if (pCapture->_callback)
	{
		pCapture->_callback(data, length, pCapture->_user_data);
	}
}