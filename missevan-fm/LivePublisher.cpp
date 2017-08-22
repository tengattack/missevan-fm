#include "StdAfx.h"
#include "LivePublisher.h"

#include "audio/AACEncoder.h"
#include "audio/MicAudioCapture.h"
#include "audio/LoopbackAudioCapture.h"

#define LPM_CREATE  WM_USER + 1
#define LPM_DATA    WM_USER + 2
#define LPM_QUIT    WM_QUIT

FILE *fp = NULL;

LivePublisher::LivePublisher()
	: m_rtmp_ptr(NULL)
	, m_encoder(NULL)
	, m_start_time(0)
	, m_samples(0)
	, m_started(false)
	, m_start_send(false)
	, m_enable_loopback(false)
	, m_mixer_thread(NULL)
	, m_mixer_threadid(NULL)
{
	memset(&m_format, 0, sizeof(m_format));
	m_format.sampleRate = 48000;
	m_format.bits = 16;
	m_format.channels = 2;
	fp = fopen("D:\\CloudMusic\\t.pcm", "wb+");
}

LivePublisher::~LivePublisher()
{
	Shutdown();
	fclose(fp);
}

uint32 LivePublisher::GetBufferLength()
{
	// about 1s
	return 96 * m_encoder->GetInputSamples() * m_format.bits / 8;
}

bool LivePublisher::IsStreaming()
{
	return m_mixer_thread != NULL;
}

bool LivePublisher::IsLoopbackEnabled()
{
	return m_enable_loopback;
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
		if ((*iter)->active) {
			active_count++;
		}
	}
	return active_count;
}

bool LivePublisher::Start(const std::string& push_url)
{
	if (!m_encoder) {
		m_encoder = new CAACEncoder();
		if (!m_encoder->Initialize(&m_format, 192000)) {
			printf("Unable to initialize aac encoder.\n");
			return false;
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

	m_samples = 0;
	m_started = false;
	m_start_send = false;
	m_push_url = push_url;
	m_mixer_event.Create(false, false);
	m_mixer_thread = CreateThread(NULL, 0, MixerProc, this, 0, &m_mixer_threadid);
	if (m_mixer_thread == NULL)
	{
		printf("Unable to create transport thread.\n");
		return false;
	}
	m_mixer_event.Wait();

	PostThreadMessage(m_mixer_threadid, LPM_CREATE, NULL, NULL);
	m_mixer_event.Wait();

	if (!m_started) {
		printf("Unable to start rtmp.\n");
		return false;
	}

	m_filter.setup(3,                   // order
				   m_format.sampleRate, // sample rate
				   4000,                // center frequency
				   880,                 // band width
				   1);                  // ripple dB

	if (!cap->capture->Start()) {
		printf("Unable to start mic capture.\n");
		Stop();
		return false;
	}

	cap->active = true;

	return true;
}

void LivePublisher::Stop()
{
	AutoLock _(m_lock);
	if (m_mixer_thread)
	{
		PostThreadMessage(m_mixer_threadid, LPM_QUIT, NULL, NULL);
		WaitForSingleObject(m_mixer_thread, INFINITE);

		CloseHandle(m_mixer_thread);
		m_mixer_thread = NULL;
		m_mixer_threadid = 0;
	}
	for (int i = 0; i < m_captures.size(); i++)
	{
		m_captures[i]->active = false;
		m_captures[i]->capture->Stop();
	}
	if (m_rtmp_ptr) {
		m_rtmp_ptr->Stop();
	}
	m_samples = 0;
	m_start_time = 0;
	m_push_url = "";
	m_started = false;
	m_start_send = false;
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
		bRet = cap->capture->Start();
		cap->active = bRet;
	}
	else
	{
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

void LivePublisher::AudioMixer(ulong mixLength)
{
	int num = GetActiveCaptureCount();
	uint8 *data = (uint8 *)malloc(mixLength);
	uint8 **sources = (uint8 **)malloc(num * sizeof(uint8 *));

	int i, j;
	LivePublisherCapture *cap_ = NULL;
	for (i = 0, j = 0; i < m_captures.size(); i++)
	{
		cap_ = m_captures[i];
		if (cap_->active) {
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
		((int16 *)data)[i] = result;
	}

	m_buf.Write(data, mixLength);

	free(sources);
	free(data);
}

void LivePublisher::_CaptureProc(uint8 *data, ulong length, LivePublisherCapture *cap)
{
	if (cap->type == kMicCapture) {
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
	}

	AutoLock _(m_lock);
	int active_count = GetActiveCaptureCount();
	// TODO: 
	// printf("capture data: %u, channel: %d\n", length, cap->captureChannel);
	// ulong t = RTMP_GetTime() - m_start_time;

	if (active_count == 1)
	{
		m_buf.Write(data, length);
	}
	else
	{
		LivePublisherCapture *cap_ = NULL;
		bool gotFirstSize = false;
		ulong minBufferSize = 0;

		cap->slice.Write(data, length);

		for (int i = 0; i < m_captures.size(); i++)
		{
			cap_ = m_captures[i];
			if (cap_->active) {
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
			for (int i = 0; i < m_captures.size(); i++)
			{
				cap_ = m_captures[i];
				if (cap_->active) {
					cap_->slice.Slice(minBufferSize);
				}
			}
		}
		else
		{
			// NO THING TO DO
			return;
		}
	}

	ulong inputSamples = m_encoder->GetInputSamples();
	ulong bufferSize = inputSamples * m_format.bits / 8;
	if (!m_start_send) {
		ulong bytesPerSec = m_format.channels * m_format.sampleRate * m_format.bits / 8;
		if (m_buf.GetBufferLen() / bytesPerSec > 5) {
			m_start_send = true;
		}
		else
		{
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
	}
}

void LivePublisher::CaptureProc(uint8 *data, ulong length, void *user_data)
{
	LivePublisherCapture *cap = (LivePublisherCapture *)user_data;
	cap->publisher->_CaptureProc(data, length, cap);
}

void LivePublisher::EncoderProc(uint8 *data, ulong length, ulong samples, void *user_data)
{
	LivePublisher *publisher = (LivePublisher *)user_data;
	AudioFormat *format = &publisher->m_format;
	ulong _samples = publisher->m_samples;

	AACData *d = (AACData *)malloc(sizeof(AACData));
	d->data = (uint8 *)malloc(length);
	d->length = length;
	d->timeoffset = _samples * 1000 / (format->channels * format->sampleRate);
	memcpy(d->data, data, length);

	//fwrite(data, 1, length, fp);

	printf("encoded: %u timeoffset: %u time: %u samples: %u\n", length, d->timeoffset, RTMP_GetTime() - publisher->m_start_time, samples);

	publisher->m_samples = _samples + samples;

	PostThreadMessage(publisher->m_mixer_threadid, LPM_DATA, NULL, (LPARAM)d);
}

DWORD LivePublisher::MixerProc(LPVOID context)
{
	LivePublisher *publisher = (LivePublisher *)context;
	bool isStreaming = true;
	MSG msg;

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
					publisher->m_start_time = RTMP_GetTime();
				}
			}
			publisher->m_mixer_event.Set();
			break;
		case LPM_DATA: {
			AACData *d = (AACData *)msg.lParam;
			publisher->m_rtmp_ptr->SendAudioAACData(d->data, d->length, d->timeoffset);
			free(d->data);
			free(d);
			break;
		}
		case LPM_QUIT:
			isStreaming = false;
			break;
		}
	}

	return 0;
}