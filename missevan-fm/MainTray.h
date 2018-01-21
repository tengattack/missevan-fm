#pragma once

#include "ui/TrayIcon.h"

class CMenu;
class Server;

class CMainTray : public CTrayIcon
{
public:
	CMainTray();
	~CMainTray();

	BOOL Create(HWND hWnd, UINT uCallbackMessage, LPCTSTR szTip, HICON icon, UINT uID, BOOL bIsNotify = FALSE, LPCTSTR szWindowTitle = NULL);
	void SetServer(Server *p_server);

	virtual LRESULT OnTrayNotification(WPARAM uID, LPARAM lEvent);
	virtual LRESULT OnTrayMenu(UINT wParam, LONG lParam);

protected:

	CMenu *m_mainMenu;
	CMenu *m_pushMenu;
	Server *m_server_ptr;

	void UpdateMenu();
};

