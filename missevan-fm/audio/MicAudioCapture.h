
#ifndef _MIC_AUDIO_CAPTURE_H_
#define _MIC_AUDIO_CAPTURE_H_
#pragma once

#include "AudioCapture.h"
#include <stdint.h>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include "AudioTransform.h"

class CMMNotificationClient;

class CMicAudioCapture :
	public CAudioCapture
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

	LONG                    _EngineLatencyInMS;

	bool HandleStreamSwitchEvent();

	//
	//  Utility functions.
	//
	bool InitializeAudioEngine();
};

#endif
