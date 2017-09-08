#include "stdafx.h"
#include "Server.h"

#include <string>
#include <base/logging.h>
#include <base/file/file.h>
#include <base/file/filedata.h>
#include <common/Buffer.h>
#include <common/strconv.h>
#include <curl/curl.h>

#include "base/common.h"
#include "base/global.h"
#include "DeviceManager.h"
#include "UserAccount.h"
#include "ChatManager.h"
#include "LivePlayer.h"
#include "LivePublisher.h"

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

std::string get_password() {
	return "test";
}

// 数据写入流 
int curl_fwrite_tomem(void *buffer, size_t size, size_t nmemb, void *stream)
{
	CBuffer *out = (CBuffer *)stream;
	if (out)
	{
		if (out->Write((unsigned char *)buffer, nmemb * size))
		{
			return nmemb;
		}
	}

	return -1;
}

Server::Server()
	: m_certs_buf(NULL)
	, m_dh_buf(NULL)
{
	m_dm_ptr = DeviceManager::GetInstance();
	m_user_ptr = UserAccount::GetInstance();
	m_cm_ptr = ChatManager::GetInstance();
	m_player_ptr = new LivePlayer();
	m_publisher_ptr = new LivePublisher();
}


Server::~Server()
{
	if (m_player_ptr) {
		m_player_ptr->Stop();
		delete m_player_ptr;
	}
	if (m_publisher_ptr) {
		m_publisher_ptr->Stop();
		delete m_publisher_ptr;
	}
	if (m_certs_buf) {
		delete m_certs_buf;
		m_certs_buf = NULL;
	}
	if (m_dh_buf) {
		delete m_dh_buf;
		m_dh_buf = NULL;
	}
}

const char *Server::S_ACTION_TEXT[] = {
	"check", "login", "logout",
	"set_volume",
	"start_pull", "stop_pull",
	"start_push", "join_connect", "stop_push",
};

Server::context_ptr Server::on_tls_init(Server* s, tls_mode mode, websocketpp::connection_hdl hdl)
{
	namespace asio = websocketpp::lib::asio;
#ifdef _DEBUG
	std::cout << "on_tls_init called with hdl: " << hdl.lock().get() << std::endl;
	std::cout << "using TLS mode: " << (mode == MOZILLA_MODERN ? "Mozilla Modern" : "Mozilla Intermediate") << std::endl;
#endif
	context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

	try {
		if (mode == MOZILLA_MODERN) {
			// Modern disables TLSv1
			ctx->set_options(asio::ssl::context::default_workarounds |
				asio::ssl::context::no_sslv2 |
				asio::ssl::context::no_sslv3 |
				asio::ssl::context::no_tlsv1 |
				asio::ssl::context::single_dh_use);
		} else {
			ctx->set_options(asio::ssl::context::default_workarounds |
				asio::ssl::context::no_sslv2 |
				asio::ssl::context::no_sslv3 |
				asio::ssl::context::single_dh_use);
		}
		//ctx->set_password_callback(bind(&get_password));
		if (s->m_certs_buf) {
			boost::asio::const_buffer buf(s->m_certs_buf->GetBuffer(), s->m_certs_buf->GetBufferLen());
			ctx->use_certificate_chain(buf);
			ctx->use_private_key(buf, asio::ssl::context::pem);
		}
		
		// Example method of generating this file:
		// `openssl dhparam -out dh.pem 2048`
		// Mozilla Intermediate suggests 1024 as the minimum size to use
		// Mozilla Modern suggests 2048 as the minimum size to use.
		if (s->m_dh_buf) {
			boost::asio::const_buffer dh_buf(s->m_dh_buf->GetBuffer(), s->m_dh_buf->GetBufferLen());
			ctx->use_tmp_dh(dh_buf);
		}
		
		std::string ciphers;

		if (mode == MOZILLA_MODERN) {
			ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
		} else {
			ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
		}

		if (SSL_CTX_set_cipher_list(ctx->native_handle(), ciphers.c_str()) != 1) {
			std::cout << "Error setting cipher list" << std::endl;
		}
	} catch (std::exception& e) {
		std::cout << "Exception: " << e.what() << std::endl;
	}
	return ctx;
}

void Server::on_http(Server* s, websocketpp::connection_hdl hdl)
{
	WebSocketServer::connection_ptr con = s->m_server.get_con_from_hdl(hdl);

	std::string body = "MissEvanFM v";
	CW2C ver(APP_VERSION);
	body += ver.c_str();

	con->set_body(body);
	con->set_status(websocketpp::http::status_code::ok);
}

Server::SAction Server::ParseActionText(const std::string &action)
{
	SAction _action = kActionNone;
	for (int i = 0; i < sizeof(S_ACTION_TEXT) / sizeof(char *); i++)
	{
		if (action == S_ACTION_TEXT[i]) {
			_action = (SAction)i;
		}
	}
	return _action;
}

const char *Server::GetActionText(SAction action)
{
	if (action == kActionNone) {
		return NULL;
	}
	return S_ACTION_TEXT[(int)action];
}

// Define a callback to handle incoming messages
void Server::onWebSocketMessage(Server* s, websocketpp::connection_hdl hdl, Server::WebSocketServer::message_ptr msg)
{
#ifdef _DEBUG
	std::cout << "on_message called with hdl: " << hdl.lock().get()
		<< " and message: " << msg->get_payload()
		<< std::endl;
#endif
	// check for a special command to instruct the server to stop listening so
	// it can be cleanly exited.
	if (msg->get_payload() == "stop") {
		//s->stop();
		return;
	}

	try {
		Json::Reader reader;
		Json::Value value;
		if (reader.parse(msg->get_payload(), value) && value.type() == Json::objectValue) {
			const std::string& action = value.get("action", "").asString();
			if (!action.empty()) {
				SAction _action = ParseActionText(action);
				if (_action == kActionNone) {
					Json::FastWriter fs;
					Json::Value ret_value;
					ret_value["code"] = kSNotFound;
					ret_value["desp"] = "action not found";
					s->m_server.send(hdl, fs.write(ret_value), msg->get_opcode());
				} else {
					LOG(INFO) << "Server action: " << action;
					s->onAction(_action, value, hdl, msg);
				}
			}
		}
	} catch (const websocketpp::lib::error_code& e) {
		LOG(ERROR) << "Echo failed because: " << e
			<< " (" << e.message() << ")";
	}
}

void Server::onWebSocketClose(Server* s, websocketpp::connection_hdl hdl)
{
	if (hdl.lock().get() == s->m_handling_hdl.lock().get()) {
		s->m_player_ptr->Stop();
		s->m_cm_ptr->LeaveRoom();
		s->m_publisher_ptr->Stop();
		s->m_user_ptr->Logout();
		s->m_handling_hdl.reset();
	}
}

uint32_t Server::GetStat()
{
	uint32_t stat = (m_user_ptr->logged() == UserAccount::kUserNone) ? kStatNone : kStatUser;
	stat |= (m_player_ptr->stat() > LivePlayer::kStatReady) ? kStatPlayer : kStatNone;
	if (m_cm_ptr->stat() != ChatManager::kChatNone) {
		stat |= kStatPushConnect;
	}
	if (m_publisher_ptr->IsStreaming()) {
		stat |= kStatPushLive;
	}
	return stat;
}

void Server::onAction(const SAction action, Json::Value &value, websocketpp::connection_hdl hdl, Server::WebSocketServer::message_ptr msg)
{
	uint32_t stat = GetStat();

	Json::FastWriter fs;
	Json::Value ret_value;

	if (action == kActionCheck) {
		Json::Value supportFeatures;
		supportFeatures.append("live");
		supportFeatures.append("connect");
		supportFeatures.append("player");

		ret_value["code"] = kSOk;
		ret_value["action"] = GetActionText(action);
		ret_value["status"] = stat;
		ret_value["version"] = APP_VERSION;
		ret_value["support"] = supportFeatures;
		m_server.send(hdl, fs.write(ret_value), msg->get_opcode());
		return;
	}

	bool allow_op = (action == kActionSetVolume);
	bool player_op = (action == kActionStartPull || action == kActionStopPull);
	bool logout_op = (action == kActionLogout);
	bool stop_push_op = (action == kActionStopPush);
	bool room_op = action == kActionStartPush || action == kActionJoinConnect || stop_push_op;
	if (room_op && !(stat & kStatUser)) {
		ret_value["code"] = kSForbidden;
		ret_value["action"] = GetActionText(action);
		ret_value["desp"] = "please login in first";
		m_server.send(hdl, fs.write(ret_value), msg->get_opcode());
		return;
	}
	if ((!room_op && !logout_op && !player_op && !allow_op && (stat & kStatUser))
			|| (!stop_push_op && !logout_op && !player_op && !allow_op && (stat & kStatPushLive || stat & kStatPushConnect))) {
		ret_value["code"] = kSForbidden;
		ret_value["action"] = GetActionText(action);
		ret_value["status"] = stat;
		ret_value["desp"] = "proxy not available now";
		m_server.send(hdl, fs.write(ret_value), msg->get_opcode());
		return;
	}

	bool params_error = false;
	bool success = false;

	if (action == kActionLogin) {
		//const uint32_t user_id = value.get("user_id", "").asUInt();
		const std::string& accid = value.get("accid", "").asString();
		const std::string& username = value.get("username", "").asString();
		const std::string& token = value.get("token", "").asString();
		if (accid.empty() || username.empty() || token.empty()) {
			params_error = true;
		} else {
			auto opcode = msg->get_opcode();
			m_user_ptr->Login(accid, username, token, [this, hdl, opcode](int code) {
				Json::FastWriter fs;
				Json::Value ret_value;
				ret_value["code"] = code;
				ret_value["action"] = GetActionText(kActionLogin);
				m_server.send(hdl, fs.write(ret_value), opcode);
				if (code == 200) {
					m_handling_hdl = hdl;
				}
			});
		}
	} else if (action == kActionLogout) {
		// stop push first
		if (stat & kStatPushLive) {
			m_publisher_ptr->Stop();
		}
		if (stat & kStatPushConnect) {
			m_cm_ptr->LeaveRoom();
		}
		m_user_ptr->Logout();
		success = true;
	} else if (action == kActionSetVolume) {
		const std::string& type = value.get("type", "").asString();
		const float volume = value.get("volume", 1).asFloat();
		if (volume >= 0 || volume <= 1) {
			if (type == "player") {
				m_dm_ptr->SetVolume(volume, false);
				m_player_ptr->SetVolume(volume);
				success = true;
			} else if (type == "mic") {
				m_dm_ptr->SetVolume(volume, true);
				success = true;
			} else {
				params_error = true;
			}
		} else {
			params_error = true;
		}
	} else if (action == kActionStartPush) {
		const uint32_t room_id = value.get("room_id", 0).asUInt();
		const std::string& push_url = value.get("push_url", "").asString();
		const std::string& type = value.get("type", "").asString();
		if (!room_id || push_url.empty()) {
			params_error = true;
		} else {
			if (stat & kStatPlayer) {
				m_player_ptr->Stop();
			}

			auto opcode = msg->get_opcode();
			if (type == "live") {
				bool bRet = m_publisher_ptr->Start(push_url);

				Json::FastWriter fs;
				Json::Value ret_value;
				ret_value["code"] = bRet ? 200 : 500;
				ret_value["action"] = GetActionText(kActionStartPush);
				m_server.send(hdl, fs.write(ret_value), opcode);
			} else if (type == "connect") {
				const std::string& room_name = value.get("room_name", "").asString();
				if (room_name.empty()) {
					params_error = true;
				} else {
					m_cm_ptr->CreateRoom(room_id, room_name, push_url, [this, hdl, opcode](int code) {
						Json::FastWriter fs;
						Json::Value ret_value;
						ret_value["code"] = code;
						ret_value["action"] = GetActionText(kActionStartPush);
						m_server.send(hdl, fs.write(ret_value), opcode);
					});
				}
			} else {
				params_error = true;
			}
		}
	} else if (action == kActionJoinConnect) {
		const uint32_t room_id = value.get("room_id", 0).asUInt();
		const std::string& room_name = value.get("room_name", "").asString();
		if (!room_id || room_name.empty()) {
			params_error = true;
		} else {
			if (stat & kStatPlayer) {
				m_player_ptr->Stop();
			}
			auto opcode = msg->get_opcode();
			m_cm_ptr->JoinRoom(room_id, room_name, [this, hdl, opcode](int code) {
				Json::FastWriter fs;
				Json::Value ret_value;
				ret_value["code"] = code;
				ret_value["action"] = GetActionText(kActionJoinConnect);
				m_server.send(hdl, fs.write(ret_value), opcode);
			});
		}
	} else if (action == kActionStopPush) {
		const uint32_t room_id = value.get("room_id", 0).asUInt();
		if (!room_id) {
			params_error = true;
		} else {
			if (stat & kStatPushLive) {
				m_publisher_ptr->Stop();
			}
			if (stat & kStatPushConnect)
			{
				m_cm_ptr->LeaveRoom();
			}
			success = true;
		}
	} else if (action == kActionStartPull) {
		const std::string& pull_url = value.get("pull_url", "").asString();
		if (pull_url.empty()) {
			params_error = true;
		} else {
			auto opcode = msg->get_opcode();
			if (!m_player_ptr->Play(pull_url, [this, hdl, opcode](int code) {
				Json::FastWriter fs;
				Json::Value ret_value;
				ret_value["code"] = code;
				ret_value["action"] = GetActionText(kActionStartPull);
				m_server.send(hdl, fs.write(ret_value), opcode);
			})) {
				ret_value["code"] = kSInternalError;
				ret_value["action"] = GetActionText(kActionStartPull);
				ret_value["desp"] = "player init failed";
				m_server.send(hdl, fs.write(ret_value), opcode);
			}
		}
	} else if (action == kActionStopPull) {
		m_player_ptr->Stop();
		success = true;
	} else {
		assert("Unexpected action");
	}

	if (params_error) {
		ret_value["code"] = kSBadRequest;
		ret_value["action"] = GetActionText(action);
		ret_value["desp"] = "params error";
		m_server.send(hdl, fs.write(ret_value), msg->get_opcode());
	} else if (success) {
		ret_value["code"] = kSOk;
		ret_value["action"] = GetActionText(action);
		m_server.send(hdl, fs.write(ret_value), msg->get_opcode());
	}
}

bool Server::fetch_certs()
{
	// 创建curl对象
	CURL *curl;
	CURLcode res;

	// curl初始化
	curl = curl_easy_init();
	
	if (curl) {
		CBuffer bufdata;

		// 设置远端地址 
		curl_easy_setopt(curl, CURLOPT_URL, APP_CERTS_URL);
		// gzip
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite_tomem);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bufdata);

		// 写入文件
		res = curl_easy_perform(curl);
		// 释放curl对象 
		curl_easy_cleanup(curl);

		if (CURLE_OK == res)
		{
			m_certs_buf = new CBuffer(bufdata);
			bufdata.SetDestoryFree(false);
		}
	}

	std::wstring dh_file_path(global::wpath);
	dh_file_path += L"data/dh.pem";

	base::CFile f;
	if (f.Open(base::kFileRead, dh_file_path.c_str())) {
		base::CFileData fd;
		if (fd.Read(f)) {
			m_dh_buf = new CBuffer();
			m_dh_buf->Write(fd.GetData(), fd.GetSize());
		}
	}

	return CURLE_OK == res;
}

bool Server::start()
{
	// download certs
	fetch_certs();

	try {
#ifdef _DEBUG
		// Set logging settings
		m_server.set_access_channels(websocketpp::log::alevel::all);
		m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
#endif
		// Initialize Asio
		m_server.init_asio();

		// Register our message handler
		m_server.set_message_handler(bind(&onWebSocketMessage, this, ::_1, ::_2));
		m_server.set_tls_init_handler(bind(&on_tls_init, this, MOZILLA_INTERMEDIATE, ::_1));
		m_server.set_http_handler(bind(&on_http, this, ::_1));
		m_server.set_close_handler(bind(&onWebSocketClose, this, ::_1));

		// Listen on port 9002
		m_server.listen(9002);

		// Start the server accept loop
		m_server.start_accept();

		// Start the ASIO io_service run loop
		m_server.run();
		return true;
	} catch (websocketpp::exception const & e) {
		std::cout << e.what() << std::endl;
	} catch (...) {
		std::cout << "other exception" << std::endl;
	}

	return false;
}

void Server::stop()
{
	if (!m_server.stopped()) {
		m_server.stop_listening();
		m_server.stop();
	}
}
