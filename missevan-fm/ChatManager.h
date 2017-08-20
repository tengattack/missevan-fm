
#ifndef _MFM_CHAT_MANAGER_H_
#define _MFM_CHAT_MANAGER_H_

#pragma once

#include <string>

class DeviceManager;

class ChatManager
{
public:

	typedef std::function<void(int)> ChatCallback;
	enum ChatStat {
		kChatNone = 0,
		kChatOwnerConnecting,
		kChatUserConnecting,
		kChatOwner,
		kChatUser,
	};
	enum ChatCbType {
		kChatCreateRoomCb = 0,
		kChatJoinRoomCb,
		kChatCbCount,
	};

	static bool Init();
	static void Cleanup();
	static ChatManager *GetInstance();

	ChatStat stat() const {
		return m_stat;
	};

	void CreateRoom(uint32_t room_id, const std::string& room_name, const std::string& push_url, ChatCallback cb = NULL);
	void JoinRoom(uint32_t room_id, const std::string& room_name, ChatCallback cb = NULL);
	void LeaveRoom();

	void CallCallback(ChatCbType type, int code);
	void ClearCallback();

protected:
	ChatManager();
	~ChatManager();

	nim::NIMVideoChatMode m_mode;
	ChatStat m_stat;
	uint32_t m_room_id;
	std::string m_room_name;
	std::string m_push_url;

	ChatCallback m_cb[kChatCbCount];

	static DeviceManager *dm;
	static ChatManager *chatManager;

	static void FormatExtString(uint32_t room_id, std::string& ext_str);

	// callbacks
	static void VChatCb(nim::NIMVideoChatSessionType type, __int64 channel_id, int code, const char *json, const void*);

	static void onCreateRoomCb(ChatManager *cm, int code, __int64 channel_id, const std::string& json_extension);
	static void onJoinRoomCb(ChatManager *cm, int code, __int64 channel_id, const std::string& json_extension);
	static void onUserJoined(ChatManager *cm, __int64 channel_id, const std::string& json_extension);
	static void onUserLeft(ChatManager *cm, __int64 channel_id, const std::string& json_extension);
};

#endif