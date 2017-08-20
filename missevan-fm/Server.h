
#ifndef _MFM_SERVER_H_
#define _MFM_SERVER_H_

#pragma once

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

#include "MainTray.h"

extern int curl_fwrite_tomem(void *buffer, size_t size, size_t nmemb, void *stream);

class UserAccount;
class ChatManager;
class DeviceManager;
class LivePlayer;
class CBuffer;

class Server
{
public:
	friend CMainTray;
	typedef websocketpp::server<websocketpp::config::asio_tls> WebSocketServer;
	typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

	// See https://wiki.mozilla.org/Security/Server_Side_TLS for more details about
	// the TLS modes. The code below demonstrates how to implement both the modern
	enum tls_mode {
		MOZILLA_INTERMEDIATE = 1,
		MOZILLA_MODERN = 2
	};

	enum SStat {
		kStatNone = 0x00,
		kStatUser = 0x01,
		kStatChat = 0x02,
		kStatPlayer = 0x04,
	};
	enum SAction {
		kActionNone = -1,
		kActionCheck = 0,
		kActionLogin,
		kActionLogout,

		kActionSetVolume,

		kActionStartPull,
		kActionStopPull,

		kActionCreateRoom,
		kActionJoinRoom,
		kActionLeaveRoom,
	};
	enum SEvent {
		kEventNetworkError = 0,
		kEventDeviceDisconnected,
		kEventPullStopped,
		kEventChatUserJoined,
		kEventChatUserLeft,
	};
	enum SResponseCode {
		kSOk = 200,
		kSBadRequest = 400,
		kSForbidden = 403,
		kSNotFound = 404,
		kSInternalError = 500,
	};

	Server();
	~Server();

	bool fetch_certs();
	bool start();
	void stop();

protected:
	WebSocketServer m_server;
	UserAccount *m_user_ptr;
	ChatManager *m_cm_ptr;
	DeviceManager *m_dm_ptr;
	LivePlayer *m_player_ptr;
	websocketpp::connection_hdl m_handling_hdl;
	CBuffer *m_certs_buf;
	CBuffer *m_dh_buf;

	uint32_t GetStat();

	void onAction(const SAction action, Json::Value &value, websocketpp::connection_hdl hdl, Server::WebSocketServer::message_ptr msg);

	static context_ptr on_tls_init(Server* s, tls_mode mode, websocketpp::connection_hdl hdl);
	static void on_http(Server* s, websocketpp::connection_hdl hdl);
	static void onWebSocketMessage(Server* s, websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg);
	static void onWebSocketClose(Server* s, websocketpp::connection_hdl hdl);

	static const char *S_ACTION_TEXT[];
	static SAction ParseActionText(const std::string &action);
	static const char *GetActionText(SAction action);
};

#endif