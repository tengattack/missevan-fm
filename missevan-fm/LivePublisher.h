
#ifndef _MFM_LIVE_PUBLISHER_H_
#define _MFM_LIVE_PUBLISHER_H_
#pragma once

#include <vector>
#include <base/basictypes.h>
#include <base/lock.h>

#include "base/types.h"
#include "audio/common.h"

#include "base/Rtmp.h"

class CAACEncoder;
class CAudioCapture;
class LivePublisher;

enum LivePublisherCaptureType {
	kMicCapture = 0,
	kLoopbackCapture,
};

typedef struct _LivePublisherCapture {
	LivePublisher *publisher;
	CAudioCapture *capture;
	LivePublisherCaptureType type;
	int captureChannel;
} LivePublisherCapture;

class LivePublisher
{
protected:
	CRtmp *m_rtmp_ptr;
	AudioFormat m_format;
	uint32 m_start_time;
	CAACEncoder *m_encoder;
	std::vector<LivePublisherCapture> m_captures;
	std::string m_push_url;
	HANDLE m_mixer_thread;
	DWORD m_mixer_threadid;
	Lock m_lock;

	bool m_enable_loopback;
	LivePublisherCapture* NewCapture(LivePublisherCaptureType type);
	LivePublisherCapture* GetCapture(LivePublisherCaptureType type);
	uint32 GetBufferLength();

	void _CaptureProc(uint8 *data, ulong length, LivePublisherCapture *cap);

	static void CALLBACK CaptureProc(uint8 *data, ulong length, void *user_data);
	static void CALLBACK EncoderProc(uint8 *data, ulong length, void *user_data);
	static DWORD CALLBACK MixerProc(LPVOID context);

public:
	LivePublisher();
	~LivePublisher();

	bool IsStreaming();

	bool Start(const std::string& push_url);
	void Stop();
	void Shutdown();

	bool EnableLookbackCapture(bool bEnable);
};

#endif
