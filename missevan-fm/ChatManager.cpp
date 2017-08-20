#include "stdafx.h"
#include "ChatManager.h"

#include "DeviceManager.h"

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
{
	ClearCallback();
}

ChatManager::~ChatManager()
{
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
		printf("[statistics]: channel_id: %I64u, code: %d, %s\n", channel_id, code, json);
#endif
		break;
	}
	}
}

void ChatManager::onJoinRoomCb(ChatManager *cm, int code, __int64 channel_id, const std::string& json_extension)
{
	//QLOG_ERR(L"JoinRoomCallback code:{0}") << code;
	//StartLiveStreamRet(code == nim::kNIMVChatConnectSuccess);
	printf("join room code: %d\n", code);

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
			assert("Unexpected chat stat");
		}
		printf("channel_id: %I64u\n", channel_id);
	} else {
		dm->EndAudioDevice();
		cm->m_stat = kChatNone;
	}
}

void ChatManager::onCreateRoomCb(ChatManager *cm, int code, __int64 channel_id, const std::string& json_extension)
{
	printf("create room code: %d\n", code);

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

void ChatManager::CreateRoom(uint32_t room_id, const std::string& room_name, const std::string& push_url, ChatCallback cb)
{
	nim::VChat::Opt2Callback _cb = std::bind(&onCreateRoomCb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	m_stat = kChatOwnerConnecting;
	m_room_id = room_id;
	m_room_name = room_name;
	m_push_url = push_url;
	m_cb[kChatCreateRoomCb] = cb;
	
	std::string ext_str;
	FormatExtString(room_id, ext_str);

	nim::VChat::CreateRoom(room_name, ext_str, "", _cb);
}

void ChatManager::JoinRoom(uint32_t room_id, const std::string& room_name, ChatCallback cb)
{
	nim::VChat::Opt2Callback _cb = std::bind(&onJoinRoomCb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	m_stat = kChatUserConnecting;
	m_room_id = room_id;
	m_room_name = room_name;
	m_push_url.clear();
	m_cb[kChatJoinRoomCb] = cb;

	Json::FastWriter fs;
	Json::Value value;
	value[nim::kNIMVChatBypassRtmp] = 1;
	value[nim::kNIMVChatRtmpRecord] = 1;
	value[nim::kNIMVChatAudioHighRate] = 1;
	std::string json_value = fs.write(value);

	dm->StartAudioDevice();
	nim::VChat::JoinRoom(m_mode, room_name, json_value, _cb);
}

void ChatManager::LeaveRoom()
{
	ClearCallback();
	m_room_id = 0;
	m_room_name.clear();
	m_push_url.clear();
	if (m_stat != kChatNone) {
		nim::VChat::End("");
		dm->EndAudioDevice();
		m_stat = kChatNone;
	}
}