#include "StdAfx.h"
#include "AACEncoder.h"


CAACEncoder::CAACEncoder()
	: CCallbackAble()
	, _hEncoder(NULL)
	, _inputSamples(0)
	, _maxOutputBytes(0)
	, _outputBuffer(NULL)
{
}

CAACEncoder::~CAACEncoder()
{
	Shutdown();
}

ulong CAACEncoder::GetInputSamples()
{
	return _inputSamples;
}

bool CAACEncoder::Initialize(AudioFormat *format, ulong targetBitrate)
{
	faacEncConfigurationPtr faacConfig;
	int result;

	if (format->bits != 16 && format->bits != 32) {
		return false;
	}

	_hEncoder = faacEncOpen(format->sampleRate, format->channels, &_inputSamples, &_maxOutputBytes);
	if (_hEncoder == NULL) {
		return false;
	}

	faacConfig = faacEncGetCurrentConfiguration(_hEncoder);

	faacConfig->mpegVersion = MPEG4;
	faacConfig->aacObjectType = LOW;
	faacConfig->inputFormat = FAAC_INPUT_16BIT;
	faacConfig->bitRate = targetBitrate;

	result = faacEncSetConfiguration(_hEncoder, faacConfig);
	if (!result) {
		return false;
	}

	_outputBuffer = (uint8 *)malloc(_maxOutputBytes);
	if (_outputBuffer == NULL) {
		Shutdown();
		return false;
	}

	return true;
}

int CAACEncoder::Encode(uint8 *data, uint32 samples)
{
	int result = faacEncEncode(_hEncoder, (int32_t *)data, samples, _outputBuffer, _maxOutputBytes);
	if (result > 0 && _callback) {
		_callback(_outputBuffer, (ulong)result, _user_data);
	}
	return result;
}

void CAACEncoder::Shutdown()
{
	if (_hEncoder) {
		faacEncClose(_hEncoder);
		_hEncoder = NULL;
	}
	if (_outputBuffer) {
		free(_outputBuffer);
		_outputBuffer = NULL;
	}
	_inputSamples = 0;
	_maxOutputBytes = 0;
	_callback = NULL;
	_user_data = NULL;
}
