
#ifndef _AUDIO_CAPTURE_H_
#define _AUDIO_CAPTURE_H_
#pragma once

#include "common.h"
#include "base/types.h"
#include <MMDeviceAPI.h>
#include <AudioPolicy.h>

class CAudioCapture;
enum AudioCaptureType {
	kCaptureData = 0,
	kCaptureDisconnected,
};
typedef void (CALLBACK *AudioCaptureCallbackProc)(AudioCaptureType type, uint8 *data, ulong length, void *user_data);

class CAudioNotificationClient: public IAudioSessionEvents, IMMNotificationClient {
public:
	CAudioNotificationClient(CAudioCapture *pCapture);
	~CAudioNotificationClient();

	STDMETHOD(OnDisplayNameChanged) (LPCWSTR /*NewDisplayName*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnIconPathChanged) (LPCWSTR /*NewIconPath*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnSimpleVolumeChanged) (float /*NewSimpleVolume*/, BOOL /*NewMute*/, LPCGUID /*EventContext*/) { return S_OK; }
	STDMETHOD(OnChannelVolumeChanged) (DWORD /*ChannelCount*/, float /*NewChannelVolumes*/[], DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnGroupingParamChanged) (LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnStateChanged) (AudioSessionState /*NewState*/) { return S_OK; };
	STDMETHOD(OnSessionDisconnected) (AudioSessionDisconnectReason DisconnectReason);
#ifdef _DEBUG
	STDMETHOD(OnDeviceStateChanged) (LPCWSTR /*DeviceId*/, DWORD /*NewState*/);
	STDMETHOD(OnDeviceAdded) (LPCWSTR /*DeviceId*/);
	STDMETHOD(OnDeviceRemoved) (LPCWSTR /*DeviceId*/);
	STDMETHOD(OnPropertyValueChanged) (LPCWSTR /*DeviceId*/, const PROPERTYKEY /*Key*/);
#else
	STDMETHOD(OnDeviceStateChanged) (LPCWSTR /*DeviceId*/, DWORD /*NewState*/) { return S_OK; }
	STDMETHOD(OnDeviceAdded) (LPCWSTR /*DeviceId*/) { return S_OK; };
	STDMETHOD(OnDeviceRemoved) (LPCWSTR /*DeviceId*/) { return S_OK; };
	STDMETHOD(OnPropertyValueChanged) (LPCWSTR /*DeviceId*/, const PROPERTYKEY /*Key*/) { return S_OK; };
#endif
	STDMETHOD(OnDefaultDeviceChanged) (EDataFlow Flow, ERole Role, LPCWSTR NewDefaultDeviceId);
	//
	//  IUnknown
	//
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);
	STDMETHOD(QueryInterface)(REFIID iid, void **pvObject);

private:
	LONG                _RefCount;

	// Private function to print device-friendly name
	HRESULT FormatDeviceName(LPCWSTR pwstrId, std::wstring *str);

protected:
	IMMDeviceEnumerator *_pEnumerator;
	CAudioCapture *_Capture;

	friend CAudioCapture;
};

class CAudioCapture : public CCallbackAble<AudioCaptureCallbackProc>
{
protected:
	// inputSamples * 96
	ulong _bufferLength;
	AudioFormat _format;
	CAudioNotificationClient *_NotifyClient;

	bool InitializeStreamSwitch(IAudioClient *audioClient, EDataFlow flow, ERole role);
	void TerminateStreamSwitch();
	// bool HandleStreamSwitchEvent();

	//
	//  Stream switch related members and methods.
	//
	bool                    _EnableStreamSwitch;
	EDataFlow               _EndpointDataFlow;
	ERole                   _EndpointRole;
	HANDLE                  _StreamSwitchEvent;          // Set when the current session is disconnected or the default device changes.
	HANDLE                  _StreamSwitchCompleteEvent;  // Set when the default device changed.
	IAudioSessionControl *  _AudioSessionControl;
	IMMDeviceEnumerator *   _DeviceEnumerator;
	bool                    _InStreamSwitch;

public:
	CAudioCapture(ulong bufferLength);
	virtual ~CAudioCapture();

	virtual bool Initialize(AudioFormat *format);
	virtual void Shutdown() = 0;
	virtual bool Start() = 0;
	virtual void Stop() = 0;

	virtual void OnDisconnected();
	virtual void OnDefaultDeviceChanged();

	friend CAudioNotificationClient;
};

#endif