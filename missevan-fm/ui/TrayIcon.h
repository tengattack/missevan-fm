/////////////////////////////////////////////////////////////////////////////
// TrayIcon.h : header file
//

#ifndef _INCLUDED_TRAYICON_H_
#define _INCLUDED_TRAYICON_H_

/////////////////////////////////////////////////////////////////////////////
// CTrayIcon window
#include <string>
#include <shellapi.h>

#define TRAY_NOTIFICATION 501
#define TRAY_TIMER_ID 502

class CMenu;

class CTrayIcon
{
// Construction/destruction
public:
	CTrayIcon();
	CTrayIcon(HWND hWnd, UINT uCallbackMessage, LPCTSTR szTip, HICON icon, UINT uID);
	virtual ~CTrayIcon();

	DWORD m_uLastTimeOut;
// Operations
public:
	BOOL Enabled() { return m_bEnabled; }
	BOOL Visible() { return !m_bHidden; }

	//Create the tray icon
	int Create(HWND hWnd, UINT uCallbackMessage, LPCTSTR szTip, HICON icon, UINT uID, BOOL bIsNotify = FALSE, LPCTSTR szWindowTitle = NULL);

	//Change or retrieve the Tooltip text
	BOOL    SetTooltipText(LPCTSTR pszTooltipText,DWORD uTimeOut = 10000);
	//BOOL    SetTooltipText(UINT nID,DWORD uTimeOut);
	void	GetTooltipText(std::wstring& text);

	//Change or retrieve the icon displayed
	BOOL  SetIcon(HICON hIcon);
	BOOL  SetIcon(LPCTSTR lpIconName);
	BOOL  SetIcon(UINT nIDResource);
	BOOL  SetStandardIcon(LPCTSTR lpIconName);
	BOOL  SetStandardIcon(UINT nIDResource);
	HICON GetIcon() const;
	void  HideIcon();
	void  ShowIcon();
	void  RemoveIcon();
	void  MoveToRight();
	void  SetMenu(CMenu *menu) {
		m_menu_ptr = menu;
	}

	//Change or retrieve the window to send notification messages to
	BOOL  SetNotificationWnd(HWND pNotifyWnd);
	BOOL  Ta_Shell_NotifyIcon(DWORD dwMessage, PNOTIFYICONDATA lpData);
	HWND GetNotificationWnd() const;

	//Default handler for tray notification message
	virtual LRESULT OnTrayNotification(WPARAM uID, LPARAM lEvent);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTrayIcon)
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL			m_bEnabled;	// does O/S support tray icon?
	BOOL			m_bHidden;	// Has the icon been hidden?
	BOOL			m_bNotify;  // Has Notify style?
	UINT			m_MenuID;
	HWND			m_hWnd;
	NOTIFYICONDATA	m_tnd;
	HANDLE			m_hTimerThread;
	UINT_PTR		m_hTimer;
	CMenu           *m_menu_ptr;
	static DWORD WINAPI TimerThread(LPVOID lparam);
	static void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
	//DECLARE_DYNAMIC(CTrayIcon)
};


#endif

/////////////////////////////////////////////////////////////////////////////
