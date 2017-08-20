
#ifndef _AUDIO_COMMON_H_
#define _AUDIO_COMMON_H_
#pragma once

#include <stdint.h>
#include <base/basictypes.h>

typedef struct _AudioFormat {
	int8      channels;
	uint32    sampleRate;
	int8      bits;
} AudioFormat;

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

#endif