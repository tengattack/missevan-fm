
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

template <class T> class CCallbackAble
{
protected:
	T _callback;
	void *_user_data;

public:
	CCallbackAble()
		: _callback(NULL)
		, _user_data(NULL)
	{
	}
	void RegisterCallback(T callback, void *user_data)
	{
		_callback = callback;
		_user_data = user_data;
	}
};

#endif