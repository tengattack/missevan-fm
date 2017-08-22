#include "StdAfx.h"
#include "MicAudioCapture.h"


CMicAudioCapture::CMicAudioCapture(uint32 bufferLength)
	: CAudioCapture(bufferLength)
	, _waveHandle(NULL)
	, _waveBuffer1(NULL)
	, _waveBuffer2(NULL)
	, _stopped(false)
{
	memset(&_waveHeader1, 0, sizeof(_waveHeader1));
	memset(&_waveHeader2, 0, sizeof(_waveHeader2));
	memset(&_waveFormat, 0, sizeof(_waveFormat));
}

CMicAudioCapture::~CMicAudioCapture()
{
	Shutdown();
}

bool CMicAudioCapture::Initialize(AudioFormat *format)
{
	CAudioCapture::_Initialize(format);

	_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	_waveFormat.nChannels = format->channels;
	_waveFormat.nSamplesPerSec = format->sampleRate;
	_waveFormat.wBitsPerSample = format->bits;
	_waveFormat.nBlockAlign = (_waveFormat.wBitsPerSample / 8) * _waveFormat.nChannels;
	_waveFormat.nAvgBytesPerSec = _waveFormat.nSamplesPerSec * _waveFormat.nBlockAlign;
	_waveFormat.cbSize = 0;

	return true;
}

void CMicAudioCapture::Shutdown()
{
	Stop();
}

bool CMicAudioCapture::Start()
{
	MMRESULT mmr = waveInOpen(&_waveHandle, WAVE_MAPPER, &_waveFormat,
		reinterpret_cast<DWORD_PTR>(waveInProc), reinterpret_cast<DWORD_PTR>(this),
		CALLBACK_FUNCTION | WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE);
	if (mmr != MMSYSERR_NOERROR)
	{
		printf("Failed to open wave in\n");
		return false;
	}

	ZeroMemory(&_waveHeader1, sizeof(_waveHeader1));
	_waveBuffer1 = new WORD[_bufferLength];
	if (_waveBuffer1 == NULL)
	{
		printf("Failed to allocate buffer for header 1\n");
		return false;
	}
	_waveHeader1.dwBufferLength = _bufferLength;
	_waveHeader1.lpData = reinterpret_cast<LPSTR>(_waveBuffer1);


	mmr = waveInPrepareHeader(_waveHandle, &_waveHeader1, sizeof(_waveHeader1));
	if (mmr != MMSYSERR_NOERROR)
	{
		printf("Failed to prepare header 1\n");
		return false;
	}

	mmr = waveInAddBuffer(_waveHandle, &_waveHeader1, sizeof(_waveHeader1));
	if (mmr != MMSYSERR_NOERROR)
	{
		printf("Failed to add buffer 1\n");
		return false;
	}

	ZeroMemory(&_waveHeader2, sizeof(_waveHeader2));
	_waveBuffer2 = new WORD[_bufferLength];
	if (_waveBuffer2 == NULL)
	{
		printf("Failed to allocate buffer for header 2\n");
		return false;
	}
	_waveHeader2.dwBufferLength = _bufferLength;
	_waveHeader2.lpData = reinterpret_cast<LPSTR>(_waveBuffer2);

	mmr = waveInPrepareHeader(_waveHandle, &_waveHeader2, sizeof(_waveHeader2));
	if (mmr != MMSYSERR_NOERROR)
	{
		printf("Failed to prepare header 2\n");
		return false;
	}

	mmr = waveInAddBuffer(_waveHandle, &_waveHeader2, sizeof(_waveHeader2));
	if (mmr != MMSYSERR_NOERROR)
	{
		printf("Failed to add buffer 2\n");
		return false;
	}

	_stopped = false;
	mmr = waveInStart(_waveHandle);
	if (mmr != MMSYSERR_NOERROR)
	{
		printf("Failed to start\n");
		return false;
	}
	return true;
}

void CMicAudioCapture::Stop()
{
	if (_waveHandle)
	{
		_stopped = true;
		MMRESULT mmr = waveInStop(_waveHandle);
		if (mmr != MMSYSERR_NOERROR)
		{
			printf("Failed to stop\n");
		}

		mmr = waveInReset(_waveHandle);
		if (mmr != MMSYSERR_NOERROR)
		{
			printf("Failed to reset\n");
		}
		mmr = waveInUnprepareHeader(_waveHandle, &_waveHeader1, sizeof(_waveHeader1));
		if (mmr != MMSYSERR_NOERROR)
		{
			printf("Failed to unprepare wave header 1\n");
		}
		delete[] _waveBuffer1;

		mmr = waveInUnprepareHeader(_waveHandle, &_waveHeader2, sizeof(_waveHeader2));
		if (mmr != MMSYSERR_NOERROR)
		{
			printf("Failed to unprepare wave header 2\n");
		}
		delete[] _waveBuffer2;

		mmr = waveInClose(_waveHandle);
		if (mmr != MMSYSERR_NOERROR)
		{
			printf("Failed to close wave handle\n");
		}
		_waveHandle = NULL;
	}
}

void CMicAudioCapture::waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	switch (uMsg)
	{
	case WIM_OPEN:
		break;
	case WIM_DATA:
	{
		MMRESULT mmr;

		HWAVEIN waveHandle = hwi;
		LPWAVEHDR waveHeader = reinterpret_cast<LPWAVEHDR>(dwParam1);
		CMicAudioCapture *pMicCapture = reinterpret_cast<CMicAudioCapture *>(dwInstance);

		if (pMicCapture->_stopped)
		{
			break;
		}

		if (pMicCapture->_callback)
		{
			pMicCapture->_callback((uint8 *)waveHeader->lpData, waveHeader->dwBytesRecorded, pMicCapture->_user_data);
		}

		if (hwi)
		{
			mmr = waveInAddBuffer(hwi, waveHeader, sizeof(WAVEHDR));
			if (mmr != MMSYSERR_NOERROR)
			{
				printf("Failed to add buffer\n");
			}
		}
		break;
	}
	case WIM_CLOSE:
		break;
	}
}