
#ifndef _MFM_LIVE_PLAYER_H_
#define _MFM_LIVE_PLAYER_H_

#pragma once

#include <string>
#include <nelp_api.h>
#include <base/lock.h>

class LivePlayer
{
public:
	typedef std::function<void(int)> PlayerCallback;
	enum PlayerStat {
		kStatError = -1,
		kStatReady = 0,
		kStatPreparing,
		kStatPrepared,
		kStatPlaying,
		kStatPaused,
	};
	enum PlayerCbType {
		kPlayCb = 0,
		kStateChangedCb,
		kCbCount,
	};
	enum PlaybackStat {
		kPlaybackError = -1,
		kPlaybackStopped = 0,
		kPlaybackPlaying,
		kPlaybackPaused,
		kPlaybackSeeking,
	};

	LivePlayer();
	~LivePlayer();

	bool Play(const std::string& play_url, PlayerCallback cb = NULL, PlayerCallback statechanged_cb = NULL);
	void Pause();
	void Stop();
	PlaybackStat GetPlaybackState();
	void SetMute(bool bMute);
	bool SetVolume(float fVolume);

	void CallCallback(PlayerCbType type, int code);
	void ClearCallback();

	PlayerStat stat() const {
		return m_stat;
	};

protected:
	_HNLPSERVICE m_hNelp;
	Lock m_lock;
	PlayerStat m_stat;
	PlayerCallback m_cb[kCbCount];

	void StopImpl();

	static std::string m_log_path;
	static void GetLogPath();

	static void MessageCb(ST_NELP_MESSAGE msg);
	static void VideoFrameCb(ST_NELP_FRAME *frame);
	static void ReleaseCb();

	void OnMessage(ST_NELP_MESSAGE msg);
};

#endif
