#include "stdafx.h"
#include "LivePlayer.h"

#include <Shlobj.h>

#include "base/common.h"

LivePlayer *g_player = NULL;

std::string LivePlayer::m_log_path;

LivePlayer::LivePlayer()
	: m_hNelp(NULL)
	, m_stat(kStatReady)
{
	g_player = this;
	GetLogPath();
	ClearCallback();
}


LivePlayer::~LivePlayer()
{
	if (m_stat > kStatReady) {
		Stop();
	}
}

void LivePlayer::GetLogPath()
{
	if (m_log_path.empty()) {
		char szPath[MAX_PATH];
		if (SHGetSpecialFolderPathA(NULL, szPath, CSIDL_LOCAL_APPDATA, FALSE)) {
			size_t len = strlen(szPath);
			if (szPath[len - 1] != '/' || szPath[len - 1] != '\\') {
				szPath[len] = '\\';
				szPath[len + 1] = 0;
			}
			m_log_path = szPath;
			m_log_path += APP_LOG_FOLDER;
		}
	}
}

void LivePlayer::MessageCb(ST_NELP_MESSAGE msg)
{
	//printf("[live player 0x%08x] %d %x %x", g_player, msg.iWhat, msg.iArg1, msg.iArg2);
	g_player->OnMessage(msg);
}

void LivePlayer::VideoFrameCb(ST_NELP_FRAME *frame)
{

}

void LivePlayer::ReleaseCb()
{

}

void LivePlayer::CallCallback(PlayerCbType type, int code)
{
	if (m_cb[type]) {
		m_cb[type](code);
		if (type != kStateChangedCb) {
			// keep callback
			m_cb[type] = NULL;
		}
	}
}

void LivePlayer::ClearCallback()
{
	for (int i = 0; i < kCbCount; i++) {
		m_cb[i] = NULL;
	}
}

void LivePlayer::OnMessage(ST_NELP_MESSAGE msg)
{
	PlayerStat s = m_stat;
	switch (msg.iWhat) {
	case NELP_MSG_ERROR:
		m_stat = kStatError;
		break;
	case NELP_MSG_PREPARED:
		m_stat = kStatPrepared;
		break;
	case NELP_MSG_VIDEO_SIZE_CHANGED:
		break;
	case NELP_MSG_BUFFERING_START:
		break;
	case NELP_MSG_BUFFERING_END:
		break;
	case NELP_MSG_COMPLETED:
		break;
	case NELP_MSG_VIDEO_RENDERING_START:
		break;
	case NELP_MSG_AUDIO_RENDERING_START:
		break;
	case NELP_MSG_PLAYBACK_STATE_CHANGED:
		CallCallback(kStateChangedCb, GetPlaybackState());
		break;
	case NELP_MSG_AUDIO_DEVICE_OPEN_FAILED:
		m_stat = kStatError;
		break;
	case NELP_MSG_SEEK_COMPLETED:
		break;
	case NELP_MSG_VIDEO_PARSE_ERROR:
		m_stat = kStatError;
		break;
	default:
		break;
	}
	if (s == kStatPreparing && m_stat != s) {
		CallCallback(kPlayCb, msg.iWhat);
	}
}

bool LivePlayer::Play(const std::string& play_url, PlayerCallback cb, PlayerCallback statechanged_cb)
{
	if (m_stat > kStatReady) {
		Stop();
	}
	AutoLock al(m_lock);

	if (m_hNelp == NULL) {
		if (Nelp_Create(m_log_path.c_str(), &m_hNelp) != NELP_OK) {
			// create failed
			m_hNelp = NULL;
			return false;
		}

		// register callback
		Nelp_RegisterGetVideoFrameCB(m_hNelp, FALSE, EN_YUV420, VideoFrameCb);
		Nelp_RegisterMessageCB(m_hNelp, MessageCb);
		Nelp_RegisterResourceReleaseSuccessCB(m_hNelp, ReleaseCb);
	}

	ST_NELP_PARAM *pstNelpParam = (ST_NELP_PARAM *)malloc(sizeof(ST_NELP_PARAM));
	pstNelpParam->paPlayUrl = (char *)play_url.c_str();
	pstNelpParam->bAutoPlay = true;
	// live streaming, low delay
	pstNelpParam->enBufferStrategy = EN_NELP_LOW_DELAY;
	if (Nelp_InitParam(m_hNelp, pstNelpParam) != NELP_OK) {
		StopImpl();
		free(pstNelpParam);
		return false;
	}

	m_cb[kPlayCb] = cb;
	m_cb[kStateChangedCb] = statechanged_cb;
	m_stat = kStatPreparing;

	Nelp_PrepareToPlay(m_hNelp);
	if (pstNelpParam) {
		free(pstNelpParam);
	}
	return true;
}

void LivePlayer::Pause()
{
	AutoLock al(m_lock);
	if (m_hNelp) {
		Nelp_Pause(m_hNelp);
		m_stat = kStatReady;
	}
}

void LivePlayer::StopImpl()
{
	if (m_hNelp) {
		Nelp_Shutdown(m_hNelp);
		m_hNelp = NULL;
	}
	m_stat = kStatReady;
	ClearCallback();
}

void LivePlayer::Stop()
{
	AutoLock al(m_lock);
	StopImpl();
}

LivePlayer::PlaybackStat LivePlayer::GetPlaybackState()
{
	PlaybackStat stat = kPlaybackError;
	if (m_hNelp) {
		EN_NELP_PLAYBACK_STATE playState = Nelp_GetPlaybackState(m_hNelp);
		return (PlaybackStat)playState;
	}
	return stat;
}

void LivePlayer::SetMute(bool bMute)
{
	if (m_stat >= kStatPrepared) {
		Nelp_SetMute(m_hNelp, bMute ? TRUE : FALSE);
	}
}

bool LivePlayer::SetVolume(float fVolume)
{
	if (m_stat >= kStatPrepared) {
		return Nelp_SetVolume(m_hNelp, fVolume) == NELP_OK;
	}
	return false;
}