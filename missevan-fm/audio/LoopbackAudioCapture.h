
#ifndef _LOOPBACK_AUDIO_CAPTURE_H_
#define _LOOPBACK_AUDIO_CAPTURE_H_
#pragma once


#include <MMDeviceAPI.h>
#include <AudioClient.h>

#include "AudioCapture.h"

class CLoopbackAudioCapture :
	public CAudioCapture
{
protected:

	IMMDevice *_ChatEndpoint;
	IAudioClient *_AudioClient;
	IAudioCaptureClient *_CaptureClient;

	HANDLE _ChatThread;
	HANDLE _ShutdownEvent;
	HANDLE _AudioSamplesReadyEvent;

	static DWORD CALLBACK WasapiThread(LPVOID Context);
public:
	CLoopbackAudioCapture(uint32 bufferLength);
	~CLoopbackAudioCapture();

	bool Initialize(AudioFormat *format);
	void Shutdown();
	bool Start();
	void Stop();
};

#endif