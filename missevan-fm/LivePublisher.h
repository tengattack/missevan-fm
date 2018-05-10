
#ifndef _MFM_LIVE_PUBLISHER_H_
#define _MFM_LIVE_PUBLISHER_H_
#pragma once

#include <vector>
#include <base/basictypes.h>
#include <base/lock.h>
#include <base/event.h>

#ifdef MIC_DENOISE
/* DSP */
#include <DSPFilters/Filter.h>
#include <DSPFilters/ChebyshevI.h>
#endif

#include "base/types.h"
#include "base/SliceBuffer.h"
#include "audio/common.h"

class CAACEncoder;
class CAudioCapture;
class LivePublisher;
enum AudioCaptureType;

enum SProvider {
	kProviderNetease = 0,
	kProviderAgora = 1,
};

namespace agora {
	namespace rtc {
		class IRtcEngine;
	}
}
class AgoraEventHandler;

enum LivePublisherCaptureType {
	kMicCapture = 0,
	kLoopbackCapture,
};

typedef struct _LivePublisherCapture {
	LivePublisher *publisher;
	CAudioCapture *capture;
	LivePublisherCaptureType type;
	int captureChannel;
	bool active;
	CSliceBuffer slice;
} LivePublisherCapture;

class LivePublisher
{
protected:
	//CRtmp *m_rtmp_ptr;
	AudioFormat m_format;
	uint32 m_start_time;
	CAACEncoder *m_encoder;

	// ÉùÍø Agora Ïà¹Ø
	agora::rtc::IRtcEngine *m_engine;
	AgoraEventHandler* m_event_handler;

	SProvider m_provider;
	std::vector<LivePublisherCapture *> m_captures;
	std::string m_push_url;
	HANDLE m_mixer_thread;
	DWORD m_mixer_threadid;
	Event m_mixer_event;
	Event m_callback_event;
	Lock m_lock;
	CSliceBuffer m_buf;
	uint32 m_time;
	bool m_started;
	bool m_start_send;
#ifdef MIC_DENOISE
	Dsp::SimpleFilter<Dsp::ChebyshevI::BandStop<3>, 2> m_filter;
#endif

	bool m_enable_loopback;
	bool m_enable_copy_mic_left;
	LivePublisherCapture* NewCapture(LivePublisherCaptureType type);
	LivePublisherCapture* GetCapture(LivePublisherCaptureType type);
	void ShutdownCapture();
	int GetActiveCaptureCount();
	uint32 GetBufferLength();
	ulong GetInputSamples();

	void _CaptureProc(uint8 *data, ulong length, LivePublisherCapture *cap);
	void AudioMixer(ulong mixLength);

	static void CALLBACK CaptureProc(AudioCaptureType type, uint8 *data, ulong length, void *user_data);
	static void CALLBACK EncoderProc(uint8 *data, ulong length, ulong samples, void *user_data);
	static DWORD CALLBACK MixerProc(LPVOID context);

public:
	typedef struct _AACData {
		uint8 *data;
		int length;
		ulong timeoffset;
	} AACData;
	typedef std::function<void(int)> ChatCallback;

	LivePublisher();
	~LivePublisher();

	bool IsStreaming();
	bool IsLoopbackEnabled();
	bool IsCopyMicLeftChannel();

	bool Start(int64_t user_id, uint32_t room_id, const std::string& room_name, const std::string& push_url, SProvider provider,
		const std::string& key,
		ChatCallback callback);
	void Stop();
	void Shutdown();

	bool EnableLookbackCapture(bool bEnable);
	bool EnableCopyMicLeftChannel(bool bEnable);
};

#endif
