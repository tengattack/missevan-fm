
#ifndef _RTMP_H_
#define _RTMP_H_
#pragma once

#include <base/basictypes.h>
#include <librtmp/rtmp_sys.h>
#include <librtmp/log.h>

#include "audio/common.h"

class CRtmp
{
protected:
	RTMP *_rtmp;

public:
	CRtmp();
	~CRtmp();

	bool Initialize(const char *push_url);

	int SendAudioAACHeader(AudioFormat *format);
	int SendAudioAACData(uint8 *buf, int len, long timeoffset);

	void Shutdown();
};

#endif
