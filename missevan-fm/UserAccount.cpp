#include "stdafx.h"
#include "UserAccount.h"

#include <base/logging.h>
#include <base/string/string_number_conversions.h>
#include <common/strconv.h>
#include "base/common.h"

UserAccount *UserAccount::userAccount = NULL;

bool UserAccount::Init()
{
	nim::SDKConfig config;
	config.database_encrypt_key_ = NIM_ENCRYPT_KEY;
#ifdef _DEBUG
	config.sdk_log_level_ = nim::kNIMSDKLogLevelPro;
#endif
	config.login_max_retry_times_ = 3;

	bool ret = nim::Client::Init(APP_KEY, APP_LOG_FOLDER, "", config);
	if (ret && userAccount == NULL) {
		userAccount = new UserAccount();
	}
	return ret;
}

void UserAccount::Cleanup()
{
	if (userAccount != NULL) {
		delete userAccount;
		userAccount = NULL;
	}
	nim::Client::Cleanup();
}

UserAccount *UserAccount::GetInstance() {
	return userAccount;
}

UserAccount::UserAccount()
	: m_logged(kUserNone)
{
	ClearCallback();
}


UserAccount::~UserAccount()
{
}


void UserAccount::OnLogoutCallback(UserAccount *userAccount, nim::NIMResCode res_code)
{
	userAccount->m_logged = kUserNone;
	LOG(INFO) << "logout done: " << (int)res_code;
}

void UserAccount::OnLoginCallback(UserAccount *userAccount, const nim::LoginRes& login_res)
{
	if (login_res.res_code_ == nim::kNIMResSuccess) {
		if (login_res.login_step_ == nim::kNIMLoginStepLogin) {
			LOG(INFO) << "login success! user(" << userAccount->m_userid << "): " << userAccount->GetUsername();
			userAccount->m_logged = kUserLogged;
			userAccount->CallCallback(kUserLoginCb, login_res.res_code_);
		} else {
			userAccount->m_logged = kUserLogging;
		}
	} else {
		LOG(INFO) << "login failed: " << (int)login_res.res_code_;
		userAccount->m_logged = kUserNone;
		userAccount->CallCallback(kUserLoginCb, login_res.res_code_);
	}
}

void UserAccount::CallCallback(UserCbType type, int code)
{
	if (m_cb[type]) {
		m_cb[type](code);
		m_cb[type] = NULL;
	}
}

void UserAccount::ClearCallback()
{
	for (int i = 0; i < kUserCbCount; i++) {
		m_cb[i] = NULL;
	}
}

void UserAccount::Login(const std::string& userid, const std::string& username, const std::string& token, UserCallback cb)
{
	m_userid = userid;
	m_username = username;
	m_logged = kUserLogging;
	m_cb[kUserLoginCb] = cb;
	nim::Client::Login(APP_KEY, userid, token, std::bind(&OnLoginCallback, this, std::placeholders::_1));
}

void UserAccount::Logout()
{
	ClearCallback();
	m_userid.clear();
	m_username.clear();
	if (m_logged != kUserNone) {
		m_logged = kUserLogging;
		nim::Client::Logout(nim::kNIMLogoutChangeAccout, std::bind(&OnLogoutCallback, this, std::placeholders::_1));
	}
}

int64_t UserAccount::GetUserId()
{
	int64_t user_id = 0;
	base::StringToInt64(m_userid, &user_id);
	return user_id;
}

int64_t UserAccount::GetAgoraUserId()
{
	// user_id * 10 + x £¨ÕâÀï x Îª 3£©
	return GetUserId() * 10 + 3;
}

std::wstring UserAccount::GetUsername()
{
	CUTF82W u2w(m_username.c_str());
	std::wstring str(u2w.c_str());
	return str;
}