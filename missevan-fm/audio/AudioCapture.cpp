#include "StdAfx.h"
#include "AudioCapture.h"


CAudioCapture::CAudioCapture(uint32 bufferLength)
	: _bufferLength(bufferLength)
{
}


CAudioCapture::~CAudioCapture()
{
}

bool CAudioCapture::Initialize(AudioFormat *format)
{
	memcpy(&_format, format, sizeof(_format));
	return true;
}

void CAudioCapture::RegisterCallback(AudioCaptureCallbackProc callback, void *user_data)
{
	_callback = callback;
	_user_data = user_data;
}