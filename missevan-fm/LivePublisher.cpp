#include "StdAfx.h"
#include "LivePublisher.h"

#include <base/logging.h>
#include <base/string/stringprintf.h>
#include <base/string/utf_string_conversions.h>
#include <base/file/file.h>
#include <base/json/values.h>
#include <base/json/json_writer.h>

#include <IAgoraRtcEngine.h>
#include <IAgoraMediaEngine.h>

#include "base/global.h"
#include "base/config.h"
#include "base/common.h"
#include "audio/MicAudioCapture.h"
#include "audio/LoopbackAudioCapture.h"

#include "UserAccount.h"
#include "Server.h"

#define LPM_CREATE       WM_USER + 1
#define LPM_DATA         WM_USER + 2
#define LPM_QUIT         WM_QUIT

#define BUFFER_TIME 5000

#ifdef _DEBUG
base::CFile file;
#endif

inline ulong min(ulong a, ulong b)
{
	return a < b ? a : b;
}

class AgoraEventHandler: public agora::rtc::IRtcEngineEventHandler
{
protected:
	LivePublisher *m_publiser;

public:
	AgoraEventHandler(LivePublisher *publisher);
	~AgoraEventHandler();

	// agora method
	virtual void onJoinChannelSuccess(const char* channel, agora::rtc::uid_t uid, int elapsed);
	virtual void onError(int err, const char* msg);

	LivePublisher::ChatCallback			m_callback;
};

AgoraEventHandler::AgoraEventHandler(LivePublisher *publisher)
	: m_publiser(publisher)
{
}

AgoraEventHandler::~AgoraEventHandler()
{
}

void AgoraEventHandler::onJoinChannelSuccess(const char* channel, agora::rtc::uid_t uid, int elapsed)
{
	std::string str;
	LOG(INFO) << base::SStringPrintf(&str, "Agora join channel success: %s, uid: %u, elapsed: %d", channel, uid, elapsed);
	m_callback(Server::kSOk);
}

void AgoraEventHandler::onError(int err, const char* msg)
{
	std::string str;
	LOG(ERROR) << base::SStringPrintf(&str, "Agora on error: %d %s", err, msg);
	static bool runOnce = true;
	if (runOnce)
	{
		m_callback(Server::kSInternalError);
		runOnce = false;
	}
}

LivePublisher::LivePublisher()
	: m_encoder(NULL)
	, m_engine(NULL)
	, m_provider(kProviderNetease)
	, m_event_handler( new AgoraEventHandler(this) )
	, m_start_time(0)
	, m_time(0)
	, m_started(false)
	, m_start_send(false)
	, m_enable_loopback(false)
	, m_enable_copy_mic_left(false)
	, m_mixer_thread(NULL)
	, m_mixer_threadid(NULL)
{
	memset(&m_format, 0, sizeof(m_format));
	m_format.sampleRate = 48000;
	m_format.bits = 16;
	m_format.channels = 2;
#ifdef _DEBUG
	std::wstring aacFile(global::wpath);
	aacFile += L"record.aac";
	file.Open(base::kFileCreate, aacFile.c_str());
#endif
}

LivePublisher::~LivePublisher()
{
#ifdef _DEBUG
	file.Close();
#endif
	Shutdown();
}

uint32 LivePublisher::GetBufferLength()
{
	// about 1s
	return 48 * GetInputSamples() * m_format.bits / 8;
}

ulong LivePublisher::GetInputSamples()
{
	if (m_provider == kProviderAgora) {
		return 2048;
	}
	else {
		return 2048;// 原先是 CAACEncoder 的代码
	}
}

bool LivePublisher::IsStreaming()
{
	return m_mixer_thread != NULL;
}

bool LivePublisher::IsLoopbackEnabled()
{
	return m_enable_loopback;
}

bool LivePublisher::IsCopyMicLeftChannel()
{
	return m_enable_copy_mic_left;
}

LivePublisherCapture* LivePublisher::NewCapture(LivePublisherCaptureType type)
{
	LivePublisherCapture *cap = GetCapture(type);

	if (!cap) {
		cap = new LivePublisherCapture;
		cap->publisher = this;
		cap->type = type;
		cap->captureChannel = type;
		cap->active = false;
		if (type == kMicCapture) {
			cap->capture = new CMicAudioCapture(GetBufferLength());
		} else if (type == kLoopbackCapture) {
			cap->capture = new CLoopbackAudioCapture(GetBufferLength());
		} else {
			return NULL;
		}
		if (!cap->capture->Initialize(&m_format)) {
			delete cap->capture;
			return NULL;
		}
		m_captures.push_back(cap);
		return cap;
	}

	return cap;
}

LivePublisherCapture* LivePublisher::GetCapture(LivePublisherCaptureType type)
{
	for (size_t i = 0; i < m_captures.size(); i++)
	{
		if (m_captures[i]->type == type) {
			return m_captures[i];
		}
	}
	return NULL;
}

int LivePublisher::GetActiveCaptureCount()
{
	int active_count = 0;
	for (auto iter = m_captures.cbegin(); iter != m_captures.cend(); iter++)
	{
		if ((*iter)->active || (*iter)->slice.GetBufferLen() > 0) {
			active_count++;
		}
	}
	return active_count;
}

bool LivePublisher::Start(int64_t user_id, uint32_t room_id, const std::string& room_name, const std::string& push_url, SProvider provider,
	const std::string& key,
	ChatCallback callback)
{
	LOG(INFO) << "Live Publisher starting...";

	// 强制使用 Agora
	m_provider = kProviderAgora;

	if (m_provider == kProviderAgora) {
		int ret;
		agora::rtc::RtcEngineContext ctx;
		ctx.appId = AGORA_APP_ID;
		// TODO: 解决内存泄漏的问题（待检查）
		m_event_handler->m_callback = callback;
		ctx.eventHandler = m_event_handler;
		m_engine = createAgoraRtcEngine();
		ret = m_engine->initialize(ctx);
		if (ret != 0) {
			LOG(ERROR) << "Agora engine init failed! error: " << ret;
			callback(ret);
			return false;
		}

		m_engine->setChannelProfile(agora::rtc::CHANNEL_PROFILE_LIVE_BROADCASTING);
		m_engine->setClientRole(agora::rtc::CLIENT_ROLE_BROADCASTER);
		m_engine->disableVideo();

		agora::rtc::RtcEngineParameters params(m_engine);
		std::wstring log_path = global::log_path;
		log_path += L"agora.log";
		params.setLogFile(WideToUTF8(log_path).c_str());
		// INFO | WARNING | ERROR | FATAL
		params.setLogFilter(15);
		params.setHighQualityAudioParameters(true, true, true);
		params.enableWebSdkInteroperability(true);
		params.setAudioProfile(agora::rtc::AUDIO_PROFILE_MUSIC_HIGH_QUALITY_STEREO, agora::rtc::AUDIO_SCENARIO_DEFAULT);
		params.setExternalAudioSource(true, m_format.sampleRate, m_format.channels);
		params.setRecordingAudioFrameParameters(m_format.sampleRate, m_format.channels, agora::rtc::RAW_AUDIO_FRAME_OP_MODE_WRITE_ONLY, GetInputSamples());

		agora::rtc::PublisherConfiguration config;
		config.width = 10;
		config.height = 10;
		config.bitrate = 100;
		config.publishUrl = push_url.c_str();
		// config.rawStreamUrl = push_url.c_str();

		std::string publisher_info;
		DictionaryValue *v = new DictionaryValue();
		v->SetBoolean("owner", config.owner);
		v->SetInteger("width", config.width);
		v->SetInteger("height", config.height);
		v->SetInteger("framerate", config.framerate);
		v->SetInteger("bitrate", config.bitrate);
		v->SetInteger("defaultLayout", config.defaultLayout);
		v->SetInteger("lifecycle", config.lifecycle);
		v->SetString("mosaicStream", config.publishUrl);
		// v->SetString("rawStream", config.rawStreamUrl);
		v->SetString("extraInfo", config.extraInfo ? config.extraInfo : "");
		v->SetBoolean("lowDelay", false);
		v->SetInteger("audiosamplerate", m_format.sampleRate);
		v->SetInteger("audiobitrate", config::audio_bitrate * 1000);
		v->SetInteger("audiochannels", m_format.channels);
		base::JSONWriter::Write(v, false, &publisher_info);
		delete v;

		// ret = m_engine->configPublisher(config);
		// if (ret != 0) {
		// 		LOG(ERROR) << "Agora engine config publisher failed! error code: " << ret;
		// }
		agora::rtc::uid_t agoraUserId = UserAccount::GetInstance()->GetAgoraUserId();
		ret = m_engine->joinChannel(key.c_str(), room_name.c_str(), publisher_info.c_str(), agoraUserId);
		if (ret != 0) {
	 		LOG(ERROR) << "Agora engine join channel failed! error: " << ret;
		}
	} else {
		// 原先是 CAACEncoder 的代码
	}

	LivePublisherCapture *cap = NewCapture(kMicCapture);
	if (!cap) {
		return false;
	}
	cap->capture->RegisterCallback(CaptureProc, cap);

	m_time = 0;
	m_started = false;
	m_start_send = false;
	m_push_url = push_url;

	m_mixer_event.Create(false, false);
	m_mixer_thread = CreateThread(NULL, 0, MixerProc, this, 0, &m_mixer_threadid);
	if (m_mixer_thread == NULL)
	{
		LOG(ERROR) << "Unable to create transport thread.";
		callback(-1);
		return false;
	}
	m_mixer_event.Wait();

	PostThreadMessage(m_mixer_threadid, LPM_CREATE, NULL, NULL);
	m_mixer_event.Wait();

	if (!m_started) {
		LOG(ERROR) << "Unable to start.";
		callback(-1);
		return false;
	}

#ifdef MIC_DENOISE
	m_filter.setup(3,                   // order
				   m_format.sampleRate, // sample rate
				   4000,                // center frequency
				   880,                 // band width
				   1);                  // ripple dB
#endif

	if (!cap->capture->Start()) {
		LOG(ERROR) << "Unable to start mic capture.";
		Stop();
		callback(-1);
		return false;
	}

	return true;
}

void LivePublisher::Stop()
{
	if (m_mixer_thread)
	{
		LOG(INFO) << "Live Publisher stopping...";
	}
	for (size_t i = 0; i < m_captures.size(); i++)
	{
		m_captures[i]->capture->Stop();
		m_captures[i]->active = false;
	}
	if (m_mixer_thread)
	{
		AutoLock _(m_lock);
		PostThreadMessage(m_mixer_threadid, LPM_QUIT, NULL, NULL);
		WaitForSingleObject(m_mixer_thread, INFINITE);

		CloseHandle(m_mixer_thread);
		m_mixer_thread = NULL;
		m_mixer_threadid = 0;
	}
	if (m_engine) {
		m_engine->leaveChannel();
		m_engine->release();
		m_engine = NULL;
	}
	if (m_event_handler) {
		delete m_event_handler;
		m_event_handler = NULL;
	}
	m_time = 0;
	m_start_time = 0;
	m_push_url = "";
	m_started = false;
	m_start_send = false;
	m_enable_loopback = false;
	m_enable_copy_mic_left = false;
	m_buf.ClearBuffer();
}

void LivePublisher::Shutdown()
{
	Stop();

	for (size_t i = 0; i < m_captures.size(); i++)
	{
		m_captures[i]->capture->Shutdown();
		delete m_captures[i]->capture;
		delete m_captures[i];
	}
	m_captures.clear();
}

bool LivePublisher::EnableLookbackCapture(bool bEnable)
{
	bool bRet;
	if (bEnable) {
		if (m_enable_loopback) {
			return true;
		}
		LivePublisherCapture *cap = NewCapture(kLoopbackCapture);
		if (!cap) {
			return false;
		}
		// lock for buffer get length
		AutoLock _(m_lock);
		cap->capture->RegisterCallback(CaptureProc, cap);
		cap->slice.ClearBuffer();
		cap->active = false;
		bRet = cap->capture->Start();
		if (!bRet) {
			LOG(ERROR) << "Live Publisher enable loopback capture failed";
		}
	} else {
		if (!m_enable_loopback) {
			return true;
		}
		LivePublisherCapture *cap = GetCapture(kLoopbackCapture);
		if (cap) {
			cap->capture->Stop();
			cap->active = false;
		}
		bRet = true;
	}
	if (bRet) {
		m_enable_loopback = bEnable;
	}
	return bRet;
}

bool LivePublisher::EnableCopyMicLeftChannel(bool bEnable)
{
	LOG(INFO) << "Live Publisher enable copy mic left channel: " << bEnable;
	m_enable_copy_mic_left = bEnable;
	return bEnable;
}

void LivePublisher::AudioMixer(ulong mixLength)
{
	size_t num = 0;

	size_t i, j;
	LivePublisherCapture *cap_ = NULL;
	for (i = 0; i < m_captures.size(); i++)
	{
		cap_ = m_captures[i];
		if (cap_->slice.GetBufferLen() >= mixLength) {
			num++;
		}
	}

	if (num <= 0) {
		return;
	}

	uint8 *data = (uint8 *)malloc(mixLength);
	uint8 **sources = (uint8 **)malloc(num * sizeof(uint8 *));
	for (i = 0, j = 0; i < m_captures.size(); i++)
	{
		cap_ = m_captures[i];
		if (cap_->slice.GetBufferLen() >= mixLength) {
			sources[j++] = cap_->slice.GetBuffer();
		}
	}

	ulong samples = mixLength / (m_format.bits / 8);
	int16 result;
	for (i = 0; i < samples; i++)
	{
		// assume all 16 bits
		result = 0;
		for (j = 0; j < num; j++) {
			result += ((int16 *)sources[j])[i];
		}
		if (result > INT16_MAX) {
			((int16 *)data)[i] = INT16_MAX;
		} else if (result < INT16_MIN) {
			((int16 *)data)[i] = INT16_MIN;
		} else {
			((int16 *)data)[i] = result;
		}
	}

	m_buf.Write(data, mixLength);

	free(sources);
	free(data);

	// slice buffer
	for (i = 0; i < m_captures.size(); i++)
	{
		cap_ = m_captures[i];
		if (cap_->slice.GetBufferLen() >= mixLength) {
			cap_->slice.Slice(mixLength);
		}
	}
}

void LivePublisher::_CaptureProc(uint8 *data, ulong length, LivePublisherCapture *cap)
{
	if (cap->type == kMicCapture) {
		if (m_enable_copy_mic_left && m_format.channels > 0) {
			int sampleBytes = m_format.bits / 8;
			int numSamples = length / sampleBytes / m_format.channels;
			for (int i = 1; i < m_format.channels; i++) {
				for (int j = 0; j < numSamples; j++) {
					memcpy(data + (j * m_format.channels + i) * sampleBytes, data + j * m_format.channels * sampleBytes, sampleBytes);
				}
			}
		}
#ifdef MIC_DENOISE
		// apply filter
		int numSamples = length * 8 / m_format.bits / m_format.channels;
		// assume 16bits
		int16 **sources = (int16 **)malloc(m_format.channels * sizeof(int16 *));
		for (int i = 0; i < m_format.channels; i++) {
			sources[i] = (int16 *)malloc(numSamples * sizeof(int16));
			for (int j = 0; j < numSamples; j++) {
				sources[i][j] = ((int16 *)data)[j * 2 + i];
			}
		}
		m_filter.process(numSamples, sources);

		for (int i = 0; i < m_format.channels; i++) {
			for (int j = 0; j < numSamples; j++) {
				((int16 *)data)[j * 2 + i] = sources[i][j];
			}
			free(sources[i]);
		}
		free(sources);
#endif
	}
	ulong t = GetTickCount() - m_start_time;

	AutoLock _(m_lock);

	/* if (data == NULL) {
		cap->active = false;
		return;
	} else {
		cap->active = true;
	} */

	int active_count = GetActiveCaptureCount();
	if (active_count == 1)
	{
		if (cap->slice.GetBufferLen() > 0) {
			m_buf.Write(cap->slice.GetBuffer(), cap->slice.GetBufferLen());
			cap->slice.ClearBuffer();
		}
		m_buf.Write(data, length);
	}
	else
	{
		cap->slice.Write(data, length);

		if (active_count > 1 && cap->type != kMicCapture) {
			return;
		}

		LivePublisherCapture *cap_ = NULL;
		bool gotFirstSize = false;
		ulong minBufferSize = 0;

		for (size_t i = 0; i < m_captures.size(); i++)
		{
			cap_ = m_captures[i];
			if (cap_->slice.GetBufferLen()) {
				if (!gotFirstSize)
				{
					// first one
					minBufferSize = (ulong)cap_->slice.GetBufferLen();
					gotFirstSize = true;
				}
				else
				{
					minBufferSize = min((ulong)cap_->slice.GetBufferLen(), minBufferSize);
				}
			}
		}

		if (minBufferSize > 0) {
			AudioMixer(minBufferSize);
			// TODO: some thing
		} else {
			// NO THING TO DO
			return;
		}
	}

	ulong inputSamples = GetInputSamples();
	ulong bufferSize = inputSamples * m_format.bits / 8;
	ulong bytesPerSec = m_format.channels * m_format.sampleRate * m_format.bits / 8;
	if (!m_start_send) {
		if (m_buf.GetBufferLen() * 1000 / bytesPerSec >= BUFFER_TIME) {
			m_start_send = true;
		} else {
			return;
		}
	}

	
	if (m_buf.GetBufferLen() > bufferSize) {
		int nbFrames = m_buf.GetBufferLen() / bufferSize;
		if (nbFrames <= 0) {
			return;
		}

		if (m_provider == kProviderAgora) {
			agora::util::AutoPtr<agora::media::IMediaEngine> mediaEngine;
			mediaEngine.queryInterface(m_engine, agora::AGORA_IID_MEDIA_ENGINE);
			// if (mediaEngine.get() != NULL) {
			// }
			for (int i = 0; i < nbFrames; i++) {
				agora::media::IAudioFrameObserver::AudioFrame frame;
				frame.type = agora::media::IAudioFrameObserver::FRAME_TYPE_PCM16;
				frame.channels = m_format.channels;
				frame.bytesPerSample = bytesPerSec;
				frame.samplesPerSec = m_format.sampleRate;
				frame.samples = inputSamples / m_format.channels;
				frame.buffer = m_buf.GetBuffer(i * bufferSize);
				frame.renderTimeMs = m_time + i * bufferSize / bytesPerSec;
				mediaEngine->pushAudioFrame(agora::media::AUDIO_RECORDING_SOURCE, &frame);
			}
		} else {
			// 原先是 CAACEncoder 的代码
		}

		ulong remain = m_buf.GetBufferLen() % bufferSize;
		if (remain > 0) {
			m_buf.Slice(nbFrames * bufferSize);
		} else {
			m_buf.ClearBuffer();
		}
		// adjust the time
		m_time = t + BUFFER_TIME + nbFrames * bufferSize / bytesPerSec;
	}
}

void LivePublisher::CaptureProc(AudioCaptureType type, uint8 *data, ulong length, void *user_data)
{
	LivePublisherCapture *cap = (LivePublisherCapture *)user_data;

	if (type == kCaptureDisconnected) {
		cap->active = false;
		return;
	}

	bool alloc = false;
	if (data == NULL) {
		data = (uint8 *)malloc(length);
		if (data == NULL) {
			return;
		}
		ZeroMemory(data, length);
		alloc = true;
	} else {
		cap->active = true;
	}
	cap->publisher->_CaptureProc(data, length, cap);
	if (alloc) {
		free(data);
	}
}

void LivePublisher::EncoderProc(uint8 *data, ulong length, ulong samples, void *user_data)
{
	LivePublisher *publisher = (LivePublisher *)user_data;
	if (!publisher->m_mixer_threadid) {
		return;
	}

	AudioFormat *format = &publisher->m_format;
	uint32 m_time = publisher->m_time;

	AACData *d = (AACData *)malloc(sizeof(AACData));
	d->data = (uint8 *)malloc(length);
	d->length = length;
	d->timeoffset = m_time;
	memcpy(d->data, data, length);

#ifdef _DEBUG
	std::string info;
	base::SStringPrintf(&info, "encoded: %u timeoffset: %u time: %u samples: %u", length, d->timeoffset, GetTickCount(), samples);
	VLOG(1) << info;
#endif

	m_time += samples * 1000 / (format->channels * format->sampleRate);
	publisher->m_time = m_time;

	PostThreadMessage(publisher->m_mixer_threadid, LPM_DATA, NULL, (LPARAM)d);
}

DWORD LivePublisher::MixerProc(LPVOID context)
{
	LivePublisher *publisher = (LivePublisher *)context;
	bool isStreaming = true;
	MSG msg;
	int sent = 0;
	int err_count = 0;
	UINT_PTR idt_reconnect = NULL;

	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_REMOVE);
	publisher->m_mixer_event.Set();

	// get msg from message queue
	while (isStreaming && GetMessage(&msg, 0, 0, 0))
	{
		switch (msg.message)
		{
		case LPM_CREATE:
			if (publisher->m_provider == kProviderAgora) {
				publisher->m_started = true;
				publisher->m_start_time = GetTickCount();
			} else {
				// 原先是 rtmp 的部分
			}
			publisher->m_mixer_event.Set();
			break;
		case LPM_DATA: {
			AACData *d = (AACData *)msg.lParam;
			// 原先是 rtmp 的部分
#ifdef _DEBUG
			std::string info;
			VLOG(1) << base::SStringPrintf(&info, "timeoffset: %u sent: %d", d->timeoffset, sent);
			file.Write(d->data, d->length);
#endif
			free(d->data);
			free(d);
			if (!sent && err_count == 0) {
				// try to reconnect
				// and wait for 3 seconds to reconnect
				err_count = 1;
				// If the function succeeds
				idt_reconnect = SetTimer(NULL, NULL, 3000, NULL);
				if (idt_reconnect == NULL) {
					PLOG(ERROR) << "Create reconnect timer failed";
				}
			}
			break;
		}
		case WM_TIMER:
			if (publisher->m_provider == kProviderAgora) {
				break;
			}
			// wParam: The timer identifier.
			if (msg.wParam == idt_reconnect) {
				LOG(INFO) << "RTMP reconnecting...";
				// 原先是 rtmp 的部分

				// free captured data if exists
				while (PeekMessage(&msg, NULL, LPM_DATA, LPM_DATA, PM_REMOVE))
				{
					AACData *d = (AACData *)msg.lParam;
					free(d->data);
					free(d);
				}

				if (err_count == 0) {
					KillTimer(NULL, idt_reconnect);
				} else {
					err_count++;
				}
			}
			break;
		case LPM_QUIT:
			isStreaming = false;
			break;
		}
	}

	// free captured data if exists
	while (PeekMessage(&msg, NULL, LPM_DATA, LPM_DATA, PM_REMOVE))
	{
		AACData *d = (AACData *)msg.lParam;
		free(d->data);
		free(d);
	}

	return 0;
}
