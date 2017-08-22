#include "StdAfx.h"
#include "AudioCapture.h"


CAudioCapture::CAudioCapture(ulong bufferLength)
	: CCallbackAble()
	, _bufferLength(bufferLength)
{
}


CAudioCapture::~CAudioCapture()
{
}

bool CAudioCapture::_Initialize(AudioFormat *format)
{
	memcpy(&_format, format, sizeof(_format));
	return true;
}
