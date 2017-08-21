#include "StdAfx.h"
#include "Rtmp.h"

#include <base/defer_ptr.h>

#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE)

#define STREAM_CHANNEL_VIDEO     0x04
#define STREAM_CHANNEL_AUDIO     0x05

CRtmp::CRtmp()
	: _rtmp(NULL)
{
}

CRtmp::~CRtmp()
{
	Shutdown();
}

bool CRtmp::Start(const char *push_url)
{
	/* set log level */
	// RTMP_LogLevel loglvl = RTMP_LOGDEBUG;
	// RTMP_LogSetLevel(loglvl);

	_rtmp = RTMP_Alloc();
	RTMP_Init(_rtmp);
	// set connection timeout, default 30s
	_rtmp->Link.timeout = 10;

	if (!RTMP_SetupURL(_rtmp, (char *)push_url))
	{
		RTMP_Log(RTMP_LOGERROR, "SetupURL Err\n");
		RTMP_Free(_rtmp);
		_rtmp = NULL;
		return false;
	}

	// if unable,the AMF command would be 'play' instead of 'publish'
	RTMP_EnableWrite(_rtmp);

	if (!RTMP_Connect(_rtmp, NULL)) {
		RTMP_Log(RTMP_LOGERROR, "Connect Err\n");
		RTMP_Free(_rtmp);
		_rtmp = NULL;
		return false;
	}

	if (!RTMP_ConnectStream(_rtmp, 0)) {
		RTMP_Log(RTMP_LOGERROR, "ConnectStream Err\n");
		RTMP_Close(_rtmp);
		RTMP_Free(_rtmp);
		_rtmp = NULL;
		return false;
	}

	return true;
}

void CRtmp::Stop()
{
	if (_rtmp) {
		RTMP_Close(_rtmp);
		RTMP_Free(_rtmp);
		_rtmp = NULL;
	}
}

int CRtmp::SendAudioAACHeader(AudioFormat *format)
{
	DEFER_INIT();
	RTMPPacket packet;
	RTMPPacket_Reset(&packet);
	RTMPPacket_Alloc(&packet, 4);
	DEFER_CATCH(RTMPPacket_Free(&packet), &packet);

	// MP3 AAC format
	packet.m_body[0] = 0xAF;
	packet.m_body[1] = 0x00;
	if (format->sampleRate == 48000)
	{
		// 0x12 0x10 => 44.1kHz
		packet.m_body[2] = 0x12;
		packet.m_body[3] = 0x10;
	}
	else if (format->sampleRate == 44100)
	{
		// 0x11 0x90 => 48kHz
		packet.m_body[2] = 0x11;
		packet.m_body[3] = 0x90;
	}
	else
	{
		// unsupport sample rate
		return -1;
	}

	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
	packet.m_hasAbsTimestamp = 0;
	packet.m_nChannel = STREAM_CHANNEL_VIDEO;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = _rtmp->m_stream_id;
	packet.m_nBodySize = 4;

	// 调用发送接口
	return RTMP_SendPacket(_rtmp, &packet, TRUE);
}

int CRtmp::SendAudioAACData(uint8 *buf, int len, long timeoffset)
{
	int nRet = -1;
	buf += 7;
	len -= 7;

	if (len > 0) {
		RTMPPacket * packet;
		unsigned char * body;

		packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE + len + 2);
		memset(packet, 0, RTMP_HEAD_SIZE);

		packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
		body = (unsigned char *)packet->m_body;

		/* AF 01 + AAC RAW data */
		body[0] = 0xAF;
		body[1] = 0x01;
		memcpy(&body[2], buf, len);

		packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
		packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
		packet->m_nBodySize = len + 2;
		packet->m_nChannel = STREAM_CHANNEL_VIDEO;
		packet->m_nTimeStamp = timeoffset;
		packet->m_hasAbsTimestamp = 0;
		packet->m_nInfoField2 = _rtmp->m_stream_id;

		// 调用发送接口
		nRet = RTMP_SendPacket(_rtmp, packet, TRUE);
		free(packet);
	}

	return nRet;
}

void CRtmp::Shutdown()
{
	Stop();
}