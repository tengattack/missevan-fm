#include "StdAfx.h"
#include "LivePublisher.h"

#include <base/logging.h>
#include <base/string/stringprintf.h>
#include <base/file/file.h>
#include "base/global.h"
#include "base/config.h"
#include "audio/AACEncoder.h"
#include "audio/MicAudioCapture.h"
#include "audio/LoopbackAudioCapture.h"

#define LPM_CREATE  WM_USER + 1
#define LPM_DATA    WM_USER + 2
#define LPM_QUIT    WM_QUIT

#define BUFFER_TIME 5000

#ifdef _DEBUG
base::CFile file;
#endif

LivePublisher::LivePublisher()
	: m_rtmp_ptr(NULL)
	, m_encoder(NULL)
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
	return 48 * m_encoder->GetInputSamples() * m_format.bits / 8;
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
	for (int i = 0; i < m_captures.size(); i++)
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

bool LivePublisher::Start(const std::string& push_url)
{
	LOG(INFO) << "Live Publisher starting...";

	if (!m_encoder) {
		m_encoder = new CAACEncoder();
		if (!m_encoder->Initialize(&m_format, config::audio_bitrate * 1000)) {
			LOG(ERROR) << "Unable to initialize aac encoder.";
			return false;
		} else {
			LOG(INFO) << "Initialize aac encoder " << config::audio_bitrate << "kbps";
		}
		m_encoder->RegisterCallback(EncoderProc, this);
	}

	LivePublisherCapture *cap = NewCapture(kMicCapture);
	if (!cap) {
		return false;
	}
	cap->capture->RegisterCallback(CaptureProc, cap);

	if (!m_rtmp_ptr) {
		m_rtmp_ptr = new CRtmp();
	}

	m_time = 0;
	m_started = false;
	m_start_send = false;
	m_push_url = push_url;
	m_mixer_event.Create(false, false);
	m_mixer_thread = CreateThread(NULL, 0, MixerProc, this, 0, &m_mixer_threadid);
	if (m_mixer_thread == NULL)
	{
		LOG(ERROR) << "Unable to create transport thread.";
		return false;
	}
	m_mixer_event.Wait();

	PostThreadMessage(m_mixer_threadid, LPM_CREATE, NULL, NULL);
	m_mixer_event.Wait();

	if (!m_started) {
		LOG(ERROR) << "Unable to start rtmp.";
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
	for (int i = 0; i < m_captures.size(); i++)
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
	if (m_rtmp_ptr) {
		m_rtmp_ptr->Stop();
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

	for (int i = 0; i < m_captures.size(); i++)
	{
		m_captures[i]->capture->Shutdown();
		delete m_captures[i]->capture;
		delete m_captures[i];
	}
	m_captures.clear();

	if (m_encoder) {
		m_encoder->Shutdown();
		delete m_encoder;
	}
	if (m_rtmp_ptr) {
		m_rtmp_ptr->Shutdown();
		delete m_rtmp_ptr;
		m_rtmp_ptr = NULL;
	}
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
	int num = 0;

	int i, j;
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
	double result;
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

		for (int i = 0; i < m_captures.size(); i++)
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
					minBufferSize = std::min((ulong)cap_->slice.GetBufferLen(), minBufferSize);
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

	ulong inputSamples = m_encoder->GetInputSamples();
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
		for (int i = 0; i < nbFrames; i++) {
			m_encoder->Encode(m_buf.GetBuffer(i * bufferSize), inputSamples);
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
	int sent;

	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_REMOVE);
	publisher->m_mixer_event.Set();

	// get msg from message queue
	while (isStreaming && GetMessage(&msg, 0, 0, 0))
	{
		switch (msg.message)
		{
		case LPM_CREATE:
			if (publisher->m_rtmp_ptr->Start(publisher->m_push_url.c_str())) {
				if (publisher->m_rtmp_ptr->SendAudioAACHeader(&publisher->m_format)) {
					publisher->m_started = true;
					publisher->m_start_time = GetTickCount();
				} else {
					LOG(ERROR) << "Send Audio AAC Header failed";
				}
			}
			publisher->m_mixer_event.Set();
			break;
		case LPM_DATA: {
			AACData *d = (AACData *)msg.lParam;
			sent = publisher->m_rtmp_ptr->SendAudioAACData(d->data, d->length, d->timeoffset);
#ifdef _DEBUG
			std::string info;
			base::SStringPrintf(&info, "timeoffset: %u sent: %d", d->timeoffset, sent);
			VLOG(1) << info;
			file.Write(d->data, d->length);
#endif
			free(d->data);
			free(d);
			break;
		}
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