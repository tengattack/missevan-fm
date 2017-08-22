
#ifndef _LOOPBACK_AUDIO_CAPTURE_H_
#define _LOOPBACK_AUDIO_CAPTURE_H_
#pragma once

#include <MMDeviceAPI.h>
#include <AudioClient.h>

#include "AudioCapture.h"
#include "AudioTransform.h"

class CLoopbackAudioCapture :
	public CAudioCapture
{
protected:

	IMMDevice *_ChatEndpoint;
	IAudioClient *_AudioClient;
	IAudioCaptureClient *_CaptureClient;
	WAVEFORMATEX _waveFormat;

	HANDLE _ChatThread;
	HANDLE _ShutdownEvent;
	HANDLE _AudioSamplesReadyEvent;

	CAudioTransform _Transform;
	bool _EnableTransform;

	static DWORD CALLBACK WasapiThread(LPVOID Context);
	static void CALLBACK TransformProc(uint8 *data, ulong length, ulong samples, void *user_data);

public:
	CLoopbackAudioCapture(uint32 bufferLength);
	~CLoopbackAudioCapture();

	bool Initialize(AudioFormat *format);
	void Shutdown();
	bool Start();
	void Stop();
};

#endif