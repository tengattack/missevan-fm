
#ifndef _MFM_USER_ACCOUNT_H_
#define _MFM_USER_ACCOUNT_H_

#pragma once

#include <string>

class UserAccount
{
public:

	typedef std::function<void(int)> UserCallback;
	enum UserLoggedStat {
		kUserNone = 0,
		kUserLogging = 1,
		kUserLogged = 2,
	};
	enum UserCbType {
		kUserLoginCb = 0,
		kUserCbCount,
	};

	static bool Init();
	static void Cleanup();
	static UserAccount *GetInstance();

	UserLoggedStat logged() const {
		return m_logged;
	}

	void Login(const std::string& userid, const std::string& username, const std::string& token, UserCallback cb = NULL);
	void Logout();

	void CallCallback(UserCbType type, int code);
	void ClearCallback();

	std::wstring GetUsername();
	int64_t GetUserId();
	int64_t GetAgoraUserId();
protected:

	UserAccount();
	~UserAccount();

	UserLoggedStat m_logged;
	std::string m_userid;
	std::string m_username;

	UserCallback m_cb[kUserCbCount];

	static UserAccount *userAccount;
	static void OnLoginCallback(UserAccount *userAccount, const nim::LoginRes& login_res);
	static void OnLogoutCallback(UserAccount *userAccount, nim::NIMResCode res_code);
};

#endif