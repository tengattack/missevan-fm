#include "stdafx.h"

#include <base/logging.h>
#include <base/json/values.h>
#include <base/json/json_writer.h>
#include <base/operation/fileselect.h>
#include <base/string/stringprintf.h>
#include <base/string/utf_string_conversions.h>
#include <common/strconv.h>

#include <IAgoraMediaEngine.h>

#include "base/global.h"
#include "base/common.h"
#include "base/config.h"
#include "DeviceManager.h"
#include "Server.h"

#include "ChatManager.h"

ChatManager *ChatManager::chatManager = NULL;
DeviceManager *ChatManager::dm = NULL;

bool ChatManager::Init()
{
	dm = DeviceManager::GetInstance();
	bool ret = nim::VChat::Init("");
	if (ret) {
		if (chatManager == NULL) {
			chatManager = new ChatManager();
		}
		nim::VChat::SetCbFunc(VChatCb);
	}
	return ret;
}

void ChatManager::Cleanup()
{
	if (chatManager != NULL) {
		delete chatManager;
		chatManager = NULL;
	}
	nim::VChat::Cleanup();
}

ChatManager *ChatManager::GetInstance() {
	return chatManager;
}

ChatManager::ChatManager()
	: m_stat(kChatNone)
	, m_mode(nim::kNIMVideoChatModeAudio)
	, m_room_id(0)
	, m_engine(NULL)
	, m_provider(kProviderNetease)
	, m_bgm(false)
{
	ClearCallback();
}

ChatManager::~ChatManager()
{
	if (m_engine) {
		m_engine->release();
		m_engine = NULL;
	}
}

void ChatManager::FormatExtString(uint32_t room_id, std::string& ext_str)
{
	Json::FastWriter fs;
	Json::Value value;

	value["room_id"] = room_id;
	value["platform"] = "win";
	
	ext_str = fs.write(value);
}

void ChatManager::VChatCb(nim::NIMVideoChatSessionType type, __int64 channel_id, int code, const char *json, const void*)
{
	ChatManager *cm = ChatManager::GetInstance();

	switch (type) {
	case nim::kNIMVideoChatSessionTypePeopleStatus:
	{
		if (code == nim::kNIMVideoChatSessionStatusJoined)
		{
			onUserJoined(cm, channel_id, json);
		}
		else if (code == nim::kNIMVideoChatSessionStatusLeaved)
		{
			onUserLeft(cm, channel_id, json);
		}
		break;
	}
	case nim::kNIMVideoChatSessionTypeInfoNotify:
	{
#ifdef _DEBUG
		std::string statistics_info;
		base::SStringPrintf(&statistics_info, "[statistics]: channel_id: %I64u, code: %d, %s\n", channel_id, code, json);
		VLOG(2) << statistics_info;
#endif
		break;
	}
	}
}

void ChatManager::onJoinRoomCb(ChatManager *cm, int code, __int64 channel_id, const std::string& json_extension)
{
	//QLOG_ERR(L"JoinRoomCallback code:{0}") << code;
	//StartLiveStreamRet(code == nim::kNIMVChatConnectSuccess);
	VLOG(1) << "join room code: " << code;

	switch (cm->m_stat) {
	case kChatOwnerConnecting:
		cm->CallCallback(kChatCreateRoomCb, code);
		break;
	case kChatUserConnecting:
		cm->CallCallback(kChatJoinRoomCb, code);
		break;
	}

	if (code == nim::kNIMVChatConnectSuccess)
	{
		switch (cm->m_stat) {
		case kChatOwnerConnecting:
			cm->m_stat = kChatOwner;
			break;
		case kChatUserConnecting:
			cm->m_stat = kChatUser;
			break;
		default:
			DLOG_ASSERT("Unexpected chat stat");
		}
		VLOG(1) << "channel_id: " << channel_id;
	} else {
		dm->EndAudioDevice();
		cm->m_stat = kChatNone;
	}
}

void ChatManager::onCreateRoomCb(ChatManager *cm, int code, __int64 channel_id, const std::string& json_extension)
{
	VLOG(1) << "create room code: " << code;

	if (code != nim::kNIMVChatConnectSuccess) {
		cm->CallCallback(kChatCreateRoomCb, code);
		cm->m_stat = kChatNone;
		return;
	}

	Json::FastWriter fs;
	Json::Value value;

	value[nim::kNIMVChatRtmpUrl] = cm->m_push_url;
	value[nim::kNIMVChatBypassRtmp] = 1;
	value[nim::kNIMVChatRtmpRecord] = 1;
	value[nim::kNIMVChatAudioHighRate] = 1;

	std::string json_value = fs.write(value);

	//QLOG_ERR(L"CreateRoomCallback code:{0}") << code;
	nim::VChat::Opt2Callback cb = std::bind(&onJoinRoomCb, cm, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	dm->StartAudioDevice();
	nim::VChat::JoinRoom(cm->m_mode, cm->m_room_name, json_value, cb);
}

void ChatManager::onUserJoined(ChatManager *cm, __int64 channel_id, const std::string& json_extension)
{

}
void ChatManager::onUserLeft(ChatManager *cm, __int64 channel_id, const std::string& json_extension)
{

}

void ChatManager::onJoinChannelSuccess(const char* channel, agora::rtc::uid_t uid, int elapsed)
{
	std::string str;
	LOG(INFO) << base::SStringPrintf(&str, "Agora join channel success: %s, uid: %u, elapsed: %d", channel, uid, elapsed);

	switch (m_stat) {
	case kChatOwnerConnecting:
		m_stat = kChatOwner;
		CallCallback(kChatCreateRoomCb, Server::kSOk);
		break;
	case kChatUserConnecting:
		m_stat = kChatUser;
		CallCallback(kChatJoinRoomCb, Server::kSOk);
		break;
	default:
		DLOG_ASSERT("Unexpected chat stat");
	}
}

void ChatManager::onError(int err, const char* msg)
{
	std::string str;
	LOG(ERROR) << base::SStringPrintf(&str, "Agora on error: %d %s", err, msg);
}

void ChatManager::CallCallback(ChatCbType type, int code)
{
	if (m_cb[type]) {
		m_cb[type](code);
		m_cb[type] = NULL;
	}
}

void ChatManager::ClearCallback()
{
	for (int i = 0; i < kChatCbCount; i++) {
		m_cb[i] = NULL;
	}
}

int ChatManager::setupAgoraEngine()
{
	int ret;
	agora::rtc::RtcEngineContext ctx;
	ctx.appId = AGORA_APP_ID;
	ctx.eventHandler = this;
	m_engine = createAgoraRtcEngine();
	ret = m_engine->initialize(ctx);
	if (ret != 0) {
		return ret;
	}

	m_engine->setChannelProfile(agora::rtc::CHANNEL_PROFILE_LIVE_BROADCASTING);
	m_engine->setClientRole(agora::rtc::CLIENT_ROLE_BROADCASTER, NULL);
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

	return 0;
}

void ChatManager::CreateRoom(int64_t user_id, uint32_t room_id, const std::string& room_name, const std::string& push_url, SProvider provider, ChatCallback cb)
{
	m_provider = provider;
	m_room_id = room_id;
	m_room_name = room_name;
	m_push_url = push_url;
	m_stat = kChatOwnerConnecting;
	m_cb[kChatCreateRoomCb] = cb;

	if (m_provider == kProviderAgora) {
		int ret = setupAgoraEngine();
		if (ret != 0) {
			LOG(ERROR) << "Agora engine init failed! error: " << ret;
			m_stat = kChatNone;
			CallCallback(kChatCreateRoomCb, Server::kSInternalError);
			return;
		}

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
		v->SetInteger("audiosamplerate", 48000);
		v->SetInteger("audiobitrate", config::audio_bitrate * 1000);
		v->SetInteger("audiochannels", 2);
		base::JSONWriter::Write(v, false, &publisher_info);
		delete v;

		// ret = m_engine->configPublisher(config);
		// if (ret != 0) {
		// 		LOG(ERROR) << "Agora engine config publisher failed! error code: " << ret;
		// }
		ret = m_engine->joinChannel(NULL, room_name.c_str(), publisher_info.c_str(), (agora::rtc::uid_t)user_id);
		if (ret != 0) {
			LOG(ERROR) << "Agora engine join channel failed! error: " << ret;
			m_stat = kChatNone;
			CallCallback(kChatCreateRoomCb, Server::kSInternalError);
			return;
		}
	} else {
		nim::VChat::Opt2Callback _cb = std::bind(&onCreateRoomCb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

		std::string ext_str;
		FormatExtString(room_id, ext_str);

		nim::VChat::CreateRoom(room_name, ext_str, "", _cb);
	}
}

void ChatManager::JoinRoom(int64_t user_id, uint32_t room_id, const std::string& room_name, SProvider provider, ChatCallback cb)
{
	m_provider = provider;
	m_room_id = room_id;
	m_room_name = room_name;
	m_push_url.clear();
	m_stat = kChatUserConnecting;
	m_cb[kChatJoinRoomCb] = cb;

	if (m_provider == kProviderAgora) {
		int ret = setupAgoraEngine();
		if (ret != 0) {
			LOG(ERROR) << "Agora engine init failed! error: " << ret;
			m_stat = kChatNone;
			CallCallback(kChatJoinRoomCb, Server::kSInternalError);
			return;
		}

		// agora::rtc::PublisherConfiguration config;
		// config.width = 10;
		// config.height = 10;
		// config.bitrate = 100;
		// config.owner = false;
		// ret = m_engine->configPublisher(config);

		ret = m_engine->joinChannel(NULL, room_name.c_str(), NULL, (agora::rtc::uid_t)user_id);
		if (ret != 0) {
			LOG(ERROR) << "Agora engine join channel failed! error: " << ret;
			m_stat = kChatNone;
			CallCallback(kChatJoinRoomCb, Server::kSInternalError);
			return;
		}
	} else {
		nim::VChat::Opt2Callback _cb = std::bind(&onJoinRoomCb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

		Json::FastWriter fs;
		Json::Value value;
		value[nim::kNIMVChatBypassRtmp] = 1;
		value[nim::kNIMVChatRtmpRecord] = 1;
		value[nim::kNIMVChatAudioHighRate] = 1;
		std::string json_value = fs.write(value);

		dm->StartAudioDevice();
		nim::VChat::JoinRoom(m_mode, room_name, json_value, _cb);
	}
}

void ChatManager::LeaveRoom()
{
	ClearCallback();

	if (m_provider == kProviderAgora) {
		if (m_engine) {
			m_engine->leaveChannel();
			m_engine->release();
			m_engine = NULL;
			m_bgm = false;
		}
	} else {
		if (m_stat != kChatNone) {
			nim::VChat::End("");
			dm->EndAudioDevice();
			m_stat = kChatNone;
		}
	}

	m_room_id = 0;
	m_room_name.clear();
	m_push_url.clear();
}

bool ChatManager::IsMicOpened()
{
	if (m_provider == kProviderAgora) {
		bool mute = false;
		if (m_engine) {
			agora::rtc::AAudioDeviceManager manger(m_engine);
			manger->getRecordingDeviceMute(&mute);
		}
		return !mute;
	} else {
		return dm->IsMicOpened();
	}
}

void ChatManager::OpenMic()
{
	if (m_provider == kProviderAgora) {
		if (m_engine) {
			agora::rtc::AAudioDeviceManager manger(m_engine);
			manger->setRecordingDeviceMute(false);
		}
		return;
	} else {
		dm->OpenMic();
	}
}

void ChatManager::CloseMic()
{
	if (m_provider == kProviderAgora) {
		if (m_engine) {
			agora::rtc::AAudioDeviceManager manger(m_engine);
			manger->setRecordingDeviceMute(true);
		}
		return;
	} else {
		dm->CloseMic();
	}
}

bool ChatManager::IsBGMEnabled()
{
	if (m_provider == kProviderAgora) {
		return m_bgm;
	} else {
		return dm->IsAudioHooked();
	}
}

bool ChatManager::EnableBGM(bool bEnable, HWND hWnd)
{
	if (m_provider == kProviderAgora) {
		if (!m_engine) {
			return true;
		}
		if (bEnable) {
			MessageBox(hWnd, L"请选择一个音频文件，本程序将会将其作为伴奏一并直播。", L"提示", MB_ICONINFORMATION | MB_OK);
			operation::CFileSelect fsel(hWnd, operation::kOpen, L"音频文件 (*.mp3)|*.mp3||", L"请选择一个音频文件");
			if (fsel.Select()) {
				agora::rtc::RtcEngineParameters params(m_engine);
				m_bgm = params.startAudioMixing(WideToUTF8(fsel.GetPath()).c_str(), false, false, -1) == 0;
				return m_bgm;
			}
		} else {
			agora::rtc::RtcEngineParameters params(m_engine);
			params.stopAudioMixing();
			m_bgm = false;
		}
		return true;
	} else {
		if (bEnable) {
			MessageBox(hWnd, L"请选择一个程序，本程序将会把其所播放的音乐一并直播，例如网易云音乐。", L"提示", MB_ICONINFORMATION | MB_OK);
			operation::CFileSelect fsel(hWnd, operation::kOpen, L"可执行文件 (*.exe)|*.exe||", L"请选择一个程序");
			if (fsel.Select()) {
				dm->StartHookAudio(WideToUTF8(fsel.GetPath()).c_str());
			}
		} else {
			dm->EndHookAudio();
		}
		return true;
	}
}
