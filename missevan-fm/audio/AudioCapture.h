
#ifndef _AUDIO_CAPTURE_H_
#define _AUDIO_CAPTURE_H_
#pragma once

#include "common.h"

class CAudioCapture;
typedef void (CALLBACK *AudioCaptureCallbackProc)(uint8 *data, uint32 length, CAudioCapture *pCapture, void *user_data);

class CAudioCapture
{
protected:
	AudioFormat _format;
	AudioCaptureCallbackProc _callback;
	void *_user_data;

	// inputSamples * 96
	uint32 _bufferLength;

public:
	CAudioCapture(uint32 bufferLength);
	virtual ~CAudioCapture();

	bool Initialize(AudioFormat *format);
	virtual void Shutdown() = 0;
	virtual bool Start() = 0;
	virtual void Stop() = 0;

	void RegisterCallback(AudioCaptureCallbackProc callback, void *user_data);
};

#endif