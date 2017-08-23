
#ifndef _AUDIO_TRANSFORM_H_
#define _AUDIO_TRANSFORM_H_
#pragma once

#include "base/types.h"
#include "common.h"

struct IMFTransform;
struct IMFMediaType;
class CAudioTransform;
typedef void (CALLBACK *AudioTransformCallbackProc)(uint8 *data, ulong length, ulong samples, void *user_data);

class CAudioTransform : public CCallbackAble<AudioTransformCallbackProc>
{
protected:
	// this is Resampler MFT
	WAVEFORMATEX _form;
	AudioFormat _to;
	IMFTransform *_pTransform;

public:
	CAudioTransform();
	~CAudioTransform();

	bool Initialize(WAVEFORMATEX *from, AudioFormat *to);
	bool Start();
	void Stop();
	void Shutdown();

	bool Encode(uint8 *data, ulong length);

	static bool Init();
	static void Cleanup();
};

#endif
