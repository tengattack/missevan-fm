
#ifndef _MIC_AUDIO_CAPTURE_H_
#define _MIC_AUDIO_CAPTURE_H_
#pragma once

#include <mmsystem.h>

#include "AudioCapture.h"
#include <stdint.h>

class CMicAudioCapture :
	public CAudioCapture
{
protected:
	HWAVEIN _waveHandle;

	WAVEHDR _waveHeader1;
	WORD *  _waveBuffer1;
	WAVEHDR _waveHeader2;
	WORD *  _waveBuffer2;

	WAVEFORMATEX _waveFormat;
	bool _stopped;

	static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
public:
	CMicAudioCapture(uint32 bufferLength);
	~CMicAudioCapture();

	bool Initialize(AudioFormat *format);
	void Shutdown();
	bool Start();
	void Stop();
};

#endif
