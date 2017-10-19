#include "StdAfx.h"
#include "AudioCapture.h"

#include <base/logging.h>
#include <boost/format.hpp>
#include <base/string/stringprintf.h>
#include <FunctionDiscoveryKeys_devpkey.h>

CAudioNotificationClient::CAudioNotificationClient(CAudioCapture *pCapture)
	: _RefCount(1)
	, _pEnumerator(NULL)
	, _Capture(pCapture)
{
	_pEnumerator = pCapture->_DeviceEnumerator;
	_pEnumerator->AddRef();
}

CAudioNotificationClient::~CAudioNotificationClient()
{
	SafeRelease(&_pEnumerator);
}

// IUnknown methods -- AddRef, Release, and QueryInterface

ULONG STDMETHODCALLTYPE CAudioNotificationClient::AddRef()
{
	return InterlockedIncrement(&_RefCount);
}

ULONG STDMETHODCALLTYPE CAudioNotificationClient::Release()
{
	ULONG ulRef = InterlockedDecrement(&_RefCount);
	if (0 == ulRef)
	{
		delete this;
	}
	return ulRef;
}

HRESULT STDMETHODCALLTYPE CAudioNotificationClient::QueryInterface(
	REFIID riid, VOID **ppvInterface)
{
	/* if (IID_IUnknown == riid)
	{
		AddRef();
		*ppvInterface = (IUnknown*)this;
	}
	else */
	if (__uuidof(IMMNotificationClient) == riid)
	{
		AddRef();
		*ppvInterface = (IMMNotificationClient*)this;
	}
	else if (__uuidof(IAudioSessionEvents) == riid)
	{
		AddRef();
		*ppvInterface = (IAudioSessionEvents*)this;
	}
	else
	{
		*ppvInterface = NULL;
		return E_NOINTERFACE;
	}
	return S_OK;
}

// Callback methods for device-event notifications.

HRESULT STDMETHODCALLTYPE CAudioNotificationClient::OnDefaultDeviceChanged(
	EDataFlow flow, ERole role,
	LPCWSTR pwstrDeviceId)
{
	if (_Capture->_EndpointDataFlow != flow || _Capture->_EndpointRole != role)
	{
		return S_OK;
	}

	wchar_t  *pszFlow = L"?????";
	wchar_t  *pszRole = L"?????";

	std::wstring str;
	FormatDeviceName(pwstrDeviceId, &str);

	switch (flow)
	{
	case eRender:
		pszFlow = L"eRender";
		break;
	case eCapture:
		pszFlow = L"eCapture";
		break;
	}

	switch (role)
	{
	case eConsole:
		pszRole = L"eConsole";
		break;
	case eMultimedia:
		pszRole = L"eMultimedia";
		break;
	case eCommunications:
		pszRole = L"eCommunications";
		break;
	}

	base::StringAppendF(&str, L"  --> New default device: flow = %s, role = %s",
		pszFlow, pszRole);

	LOG(INFO) << str;
	_Capture->OnDefaultDeviceChanged();
	return S_OK;
}


HRESULT STDMETHODCALLTYPE CAudioNotificationClient::OnSessionDisconnected(
	AudioSessionDisconnectReason DisconnectReason)
{
	char *pszReason = "?????";

	switch (DisconnectReason)
	{
	case DisconnectReasonDeviceRemoval:
		pszReason = "DisconnectReasonDeviceRemoval";
		break;
	case DisconnectReasonServerShutdown:
		pszReason = "DisconnectReasonDeviceRemoval";
		break;
	case DisconnectReasonFormatChanged:
		pszReason = "DisconnectReasonDeviceRemoval";
		break;
	case DisconnectReasonSessionLogoff:
		pszReason = "DisconnectReasonDeviceRemoval";
		break;
	case DisconnectReasonSessionDisconnected:
		pszReason = "DisconnectReasonDeviceRemoval";
		break;
	case DisconnectReasonExclusiveModeOverride:
		pszReason = "DisconnectReasonDeviceRemoval";
		break;
	}

	LOG(INFO) << "Audio Session Disconnected: " << pszReason << boost::format(" (%d)") % (int)DisconnectReason;
	_Capture->OnDisconnected();
	return S_OK;
}

#ifdef _DEBUG
HRESULT STDMETHODCALLTYPE CAudioNotificationClient::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
	std::wstring str;
	FormatDeviceName(pwstrDeviceId, &str);

	str += L"  --> Added device";

	DLOG(INFO) << str;
	return S_OK;
};

HRESULT STDMETHODCALLTYPE CAudioNotificationClient::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
	std::wstring str;
	FormatDeviceName(pwstrDeviceId, &str);

	str += L"  --> Removed device";

	DLOG(INFO) << str;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CAudioNotificationClient::OnDeviceStateChanged(
	LPCWSTR pwstrDeviceId,
	DWORD dwNewState)
{
	wchar_t  *pszState = L"?????";
	std::wstring str;

	FormatDeviceName(pwstrDeviceId, &str);

	switch (dwNewState)
	{
	case DEVICE_STATE_ACTIVE:
		pszState = L"ACTIVE";
		break;
	case DEVICE_STATE_DISABLED:
		pszState = L"DISABLED";
		break;
	case DEVICE_STATE_NOTPRESENT:
		pszState = L"NOTPRESENT";
		break;
	case DEVICE_STATE_UNPLUGGED:
		pszState = L"UNPLUGGED";
		break;
	}

	base::StringAppendF(&str, L"  --> New device state is DEVICE_STATE_%s (0x%8.8x)",
		pszState, dwNewState);

	DLOG(INFO) << str;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CAudioNotificationClient::OnPropertyValueChanged(
	LPCWSTR pwstrDeviceId,
	const PROPERTYKEY key)
{
	std::wstring str;
	FormatDeviceName(pwstrDeviceId, &str);

	base::StringAppendF(&str, L"  --> Changed device property "
		L"{%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x}#%d",
		key.fmtid.Data1, key.fmtid.Data2, key.fmtid.Data3,
		key.fmtid.Data4[0], key.fmtid.Data4[1],
		key.fmtid.Data4[2], key.fmtid.Data4[3],
		key.fmtid.Data4[4], key.fmtid.Data4[5],
		key.fmtid.Data4[6], key.fmtid.Data4[7],
		key.pid);

	DLOG(INFO) << str;
	return S_OK;
}
#endif

// Given an endpoint ID string, print the friendly device name.
HRESULT CAudioNotificationClient::FormatDeviceName(LPCWSTR pwstrId, std::wstring *str)
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

	base::SStringPrintf(str, L"----- Device name: \"%s\""
		L"  Endpoint ID string: \"%s\"",
		(hr == S_OK) ? varString.pwszVal : L"null device",
		(pwstrId != NULL) ? pwstrId : L"null ID");

	PropVariantClear(&varString);

	SafeRelease(&pProps);
	SafeRelease(&pDevice);
	return hr;
}

CAudioCapture::CAudioCapture(ulong bufferLength)
	: CCallbackAble()
	, _bufferLength(bufferLength)
	, _NotifyClient(NULL)
	, _StreamSwitchEvent(NULL)
	, _StreamSwitchCompleteEvent(NULL)
	, _AudioSessionControl(NULL)
	, _DeviceEnumerator(NULL)
	, _EnableStreamSwitch(false)
	, _InStreamSwitch(false)
	, _EndpointDataFlow(eRender)
	, _EndpointRole(eMultimedia)
{
}


CAudioCapture::~CAudioCapture()
{
	SafeRelease(&_NotifyClient);

	if (_StreamSwitchEvent)
	{
		CloseHandle(_StreamSwitchEvent);
		_StreamSwitchEvent = NULL;
	}
}

bool CAudioCapture::Initialize(AudioFormat *format)
{
	memcpy(&_format, format, sizeof(_format));

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

	return true;
}


//
//  Initialize the stream switch logic.
//
bool CAudioCapture::InitializeStreamSwitch(IAudioClient *audioClient, EDataFlow flow, ERole role)
{
	if (_NotifyClient == NULL) {
		_NotifyClient = new CAudioNotificationClient(this);
	}

	TerminateStreamSwitch();

	_EndpointDataFlow = flow;
	_EndpointRole = role;

	HRESULT hr = audioClient->GetService(IID_PPV_ARGS(&_AudioSessionControl));
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
	hr = _AudioSessionControl->RegisterAudioSessionNotification(_NotifyClient);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to register for session notifications: " << boost::format("0x%08x") % hr;
		return false;
	}

	hr = _DeviceEnumerator->RegisterEndpointNotificationCallback(_NotifyClient);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to register for endpoint notifications: " << boost::format("0x%08x") % hr;
		return false;
	}

	_EnableStreamSwitch = true;
	return true;
}


void CAudioCapture::TerminateStreamSwitch()
{
	if (!_EnableStreamSwitch)
	{
		return;
	}
	_EnableStreamSwitch = false;

	HRESULT hr = _AudioSessionControl->UnregisterAudioSessionNotification(_NotifyClient);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to unregister for session notifications: " << boost::format("0x%08x") % hr;
	}

	_DeviceEnumerator->UnregisterEndpointNotificationCallback(_NotifyClient);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to unregister for endpoint notifications: " << boost::format("0x%08x") % hr;
	}

	if (_StreamSwitchCompleteEvent)
	{
		CloseHandle(_StreamSwitchCompleteEvent);
		_StreamSwitchCompleteEvent = NULL;
	}

	SafeRelease(&_AudioSessionControl);
}

void CAudioCapture::OnDisconnected()
{
	_InStreamSwitch = true;
	SetEvent(_StreamSwitchEvent);

	if (_callback) {
		_callback(kCaptureDisconnected, NULL, NULL, _user_data);
	}
}

void CAudioCapture::OnDefaultDeviceChanged()
{
	_InStreamSwitch = true;
	SetEvent(_StreamSwitchEvent);
}