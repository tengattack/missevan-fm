#include "StdAfx.h"
#include "LivePublisher.h"

#include "audio/AACEncoder.h"
#include "audio/MicAudioCapture.h"
#include "audio/LoopbackAudioCapture.h"

#define LPM_QUIT WM_QUIT

LivePublisher::LivePublisher()
	: m_rtmp_ptr(NULL)
	, m_encoder(NULL)
	, m_start_time(0)
	, m_enable_loopback(false)
	, m_mixer_thread(NULL)
	, m_mixer_threadid(NULL)
{
	memset(&m_format, 0, sizeof(m_format));
	m_format.sampleRate = 48000;
	m_format.bits = 16;
	m_format.channels = 2;
}

LivePublisher::~LivePublisher()
{
	Shutdown();
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

LivePublisherCapture* LivePublisher::NewCapture(LivePublisherCaptureType type)
{
	LivePublisherCapture *cap_ptr = GetCapture(kMicCapture);

	if (!cap_ptr) {
		LivePublisherCapture cap;
		cap.publisher = this;
		cap.type = kMicCapture;
		cap.captureChannel = kMicCapture;
		cap.capture = new CMicAudioCapture(GetBufferLength());
		if (!cap.capture->Initialize(&m_format)) {
			delete cap.capture;
			return NULL;
		}
		m_captures.push_back(cap);
		return &m_captures[m_captures.size() - 1];
	}

	return cap_ptr;
}

LivePublisherCapture* LivePublisher::GetCapture(LivePublisherCaptureType type)
{
	for (int i = 0; i < m_captures.size(); i++)
	{
		if (m_captures[i].type == type) {
			return &m_captures[i];
		}
	}
	return NULL;
}

bool LivePublisher::Start(const std::string& push_url)
{
	if (!m_encoder) {
		m_encoder = new CAACEncoder();
		m_encoder->Initialize(&m_format, 192000);
	}

	LivePublisherCapture *cap = NewCapture(kMicCapture);
	if (!cap) {
		return false;
	}
	cap->capture->RegisterCallback(CaptureProc, cap);

	if (!m_rtmp_ptr) {
		m_rtmp_ptr = new CRtmp();
	}

	m_push_url = push_url;
	m_mixer_thread = CreateThread(NULL, 0, MixerProc, this, 0, &m_mixer_threadid);
	if (m_mixer_thread == NULL)
	{
		printf("Unable to create transport thread.\n");
		return false;
	}

	if (!cap->capture->Start()) {
		printf("Unable to start mic capture.\n");
		Stop();
		return false;
	}

	return true;
}

void LivePublisher::Stop()
{
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
		m_captures[i].capture->Stop();
	}
	if (m_rtmp_ptr) {
		m_rtmp_ptr->Stop();
	}
	m_start_time = 0;
	m_push_url = "";
}

void LivePublisher::Shutdown()
{
	Stop();
	for (int i = 0; i < m_captures.size(); i++)
	{
		m_captures[i].capture->Shutdown();
		delete m_captures[i].capture;
	}
	if (m_encoder) {
		m_encoder->Shutdown();
		delete m_encoder;
	}
	if (m_rtmp_ptr) {
		m_rtmp_ptr->Shutdown();
		delete m_rtmp_ptr;
		m_rtmp_ptr = NULL;
	}
	m_captures.clear();
	m_start_time = 0;
	m_push_url = "";
}

bool LivePublisher::EnableLookbackCapture(bool bEnable)
{
	if (bEnable) {
		LivePublisherCapture *cap = NewCapture(kLoopbackCapture);
		if (!cap) {
			return false;
		}
		cap->capture->RegisterCallback(CaptureProc, cap);
	}
	else
	{
		LivePublisherCapture *cap = GetCapture(kLoopbackCapture);
		if (cap) {
			cap->capture->Stop();
		}
	}
	m_enable_loopback = bEnable;
}

void LivePublisher::_CaptureProc(uint8 *data, ulong length, LivePublisherCapture *cap)
{
	AutoLock _(m_lock);
	// TODO: 
}

void LivePublisher::CaptureProc(uint8 *data, ulong length, void *user_data)
{
	LivePublisherCapture *cap = (LivePublisherCapture *)user_data;
	cap->publisher->_CaptureProc(data, length, cap);
}

void LivePublisher::EncoderProc(uint8 *data, ulong length, void *user_data)
{
	// PostThreadMessage()
}

DWORD LivePublisher::MixerProc(LPVOID context)
{
	LivePublisher *publisher = (LivePublisher *)context;
	MSG msg;

	publisher->m_rtmp_ptr->Start(publisher->m_push_url.c_str());
	publisher->m_rtmp_ptr->SendAudioAACHeader(&publisher->m_format);
	publisher->m_start_time = RTMP_GetTime();

	while (true)
	{
		// get msg from message queue
		if (GetMessage(&msg, 0, 0, 0))
		{
			switch (msg.message)
			{
			case LPM_QUIT:
				break;
			}
		}
	}
}