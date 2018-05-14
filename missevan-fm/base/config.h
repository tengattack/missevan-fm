
#ifndef _MFM_CONFIG_H_
#define _MFM_CONFIG_H_
#pragma once

namespace config {

	extern std::string proxy_url;
	// bitrate in kbps
	extern int audio_bitrate;

	bool read();
};

#endif