
#ifndef _MIC_AUDIO_CAPTURE_H_
#define _MIC_AUDIO_CAPTURE_H_
#pragma once

#include "AudioCapture.h"
#include <stdint.h>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include "AudioTransform.h"

class CMMNotificationClient;

class CMicAudioCapture :
	public CAudioCapture /*, IAudioSessionEvents, IMMNotificationClient */
{
protected:

	WAVEFORMATEX _waveFormat;
	CMMNotificationClient *_notifyClient;
	bool _stopped;

public:
	CMicAudioCapture(uint32 bufferLength);
	~CMicAudioCapture();

	bool Initialize(AudioFormat *format);
	void Shutdown();
	bool Start();
	void Stop();

private:
	LONG                _RefCount;
	//
	//  Core Audio Capture member variables.
	//
	IMMDevice *         _Endpoint;
	IAudioClient *      _AudioClient;
	IAudioCaptureClient *_CaptureClient;

	HANDLE              _CaptureThread;
	HANDLE              _ShutdownEvent;
	HANDLE              _AudioSamplesReadyEvent;
	size_t              _FrameSize;
	UINT32 _BufferSize;
	CAudioTransform _Transform;
	bool _EnableTransform;

	//
	//  Capture buffer management.
	//
	BYTE *_CaptureBuffer;
	size_t _CaptureBufferSize;
	size_t _CurrentCaptureIndex;

	static void CALLBACK TransformProc(uint8 *data, ulong length, ulong samples, void *user_data);
	static DWORD __stdcall WASAPICaptureThread(LPVOID Context);
	DWORD DoCaptureThread();
	//
	//  Stream switch related members and methods.
	//
	bool                    _EnableStreamSwitch;
	ERole                   _EndpointRole;
	HANDLE                  _StreamSwitchEvent;          // Set when the current session is disconnected or the default device changes.
	HANDLE                  _StreamSwitchCompleteEvent;  // Set when the default device changed.
	IAudioSessionControl *  _AudioSessionControl;
	IMMDeviceEnumerator *   _DeviceEnumerator;
	LONG                    _EngineLatencyInMS;
	bool                    _InStreamSwitch;

	bool InitializeStreamSwitch();
	void TerminateStreamSwitch();
	// bool HandleStreamSwitchEvent();

	STDMETHOD(OnDisplayNameChanged) (LPCWSTR /*NewDisplayName*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnIconPathChanged) (LPCWSTR /*NewIconPath*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnSimpleVolumeChanged) (float /*NewSimpleVolume*/, BOOL /*NewMute*/, LPCGUID /*EventContext*/) { return S_OK; }
	STDMETHOD(OnChannelVolumeChanged) (DWORD /*ChannelCount*/, float /*NewChannelVolumes*/[], DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnGroupingParamChanged) (LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/) { return S_OK; };
	STDMETHOD(OnStateChanged) (AudioSessionState /*NewState*/) { return S_OK; };
	// STDMETHOD(OnSessionDisconnected) (AudioSessionDisconnectReason DisconnectReason);
	STDMETHOD(OnDeviceStateChanged) (LPCWSTR /*DeviceId*/, DWORD /*NewState*/) { return S_OK; }
	STDMETHOD(OnDeviceAdded) (LPCWSTR /*DeviceId*/) { return S_OK; };
	STDMETHOD(OnDeviceRemoved) (LPCWSTR /*DeviceId(*/) { return S_OK; };
	// STDMETHOD(OnDefaultDeviceChanged) (EDataFlow Flow, ERole Role, LPCWSTR NewDefaultDeviceId);
	STDMETHOD(OnPropertyValueChanged) (LPCWSTR /*DeviceId*/, const PROPERTYKEY /*Key*/) { return S_OK; };
	//
	//  IUnknown
	//
	// STDMETHOD(QueryInterface)(REFIID iid, void **pvObject);

	//
	//  Utility functions.
	//
	bool InitializeAudioEngine();
};

#endif
