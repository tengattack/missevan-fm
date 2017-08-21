
#ifndef _MIC_AUDIO_AAC_ENCODER_H_
#define _MIC_AUDIO_AAC_ENCODER_H_
#pragma once

#include <base/basictypes.h>
#include <faac.h>

#include "base/types.h"
#include "common.h"

typedef void (CALLBACK *EncoderCallbackProc)(uint8 *data, ulong length, void *user_data);

class CAACEncoder : public CCallbackAble<EncoderCallbackProc>
{
protected:
	faacEncHandle _hEncoder;
	ulong _inputSamples;
	ulong _maxOutputBytes;
	uint8 *_outputBuffer;

public:
	CAACEncoder();
	~CAACEncoder();

	ulong GetInputSamples();

	bool Initialize(AudioFormat *format, ulong targetBitrate);
	int Encode(uint8 *data, uint32 samples);
	void Shutdown();
};

#endif
