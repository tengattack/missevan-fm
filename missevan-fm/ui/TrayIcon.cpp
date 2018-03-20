/////////////////////////////////////////////////////////////////////////////
// TrayIcon.cpp : implementation file
//
// This is a conglomeration of ideas from the MSJ "Webster" application,
// sniffing round the online docs, and from other implementations such
// as PJ Naughter's "CTrayIconifyIcon" (http://indigo.ie/~pjn/ntray.html)
// especially the "CTrayIcon::OnTrayNotification" member function.
//
// This class is a light wrapper around the windows system tray stuff. It
// adds an icon to the system tray with the specified ToolTip text and 
// callback notification value, which is sent back to the Parent window.
//
// The tray icon can be instantiated using either the constructor or by
// declaring the object and creating (and displaying) it later on in the
// program. eg.
//
//		CTrayIcon m_TrayIcon;	// Member variable of some class
//		
//		... 
//		// in some member function maybe...
//		m_TrayIcon.Create(pParentWnd, WM_MY_NOTIFY, "Click here", 
//						  hIcon, nTrayIconID);
//
// Clobbered together by Chris Maunder.
// 
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#ifdef NOSTDAFX
#include <Windows.h>
#include <tchar.h>
#endif
#include "TrayIcon.h"
#include "Menu.h"
#include "../base/global.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define VERIFY
#define ASSERT
//IMPLEMENT_DYNAMIC(CTrayIcon, CObject)

/////////////////////////////////////////////////////////////////////////////
// CTrayIcon construction/creation/destruction

CTrayIcon::CTrayIcon()
	: m_menu_ptr(NULL)
	, m_hTimer(NULL)
{
	memset(&m_tnd, 0, sizeof(m_tnd));
	m_bEnabled = FALSE;
	m_bHidden  = FALSE;
	m_MenuID = 0;
	m_hWnd = NULL;
	m_hTimerThread = NULL;
	m_uLastTimeOut = NULL;
}

CTrayIcon::CTrayIcon(HWND hWnd, UINT uCallbackMessage, LPCTSTR szToolTip, 
					 HICON icon, UINT uID)
	: m_menu_ptr(NULL)
{
	Create(hWnd, uCallbackMessage, szToolTip, icon, uID);
	m_bHidden = FALSE;
}

BOOL CTrayIcon::Ta_Shell_NotifyIcon(DWORD dwMessage, PNOTIFYICONDATA lpData)
{
	return Shell_NotifyIcon(dwMessage, lpData);
}

BOOL CTrayIcon::Create(HWND hWnd, UINT uCallbackMessage, LPCTSTR szToolTip, 
					   HICON icon, UINT uID, BOOL bIsNotify, LPCTSTR szWindowTitle)
{
	// if bIsNotify == TRUE
	// Add these code to stdafx.h
	/*
	#ifndef _WIN32_IE // 允许使用 IE 4.0 或更高版本的特定功能。
	#define _WIN32_IE 0x0500 //为 IE 5.0 及更新版本改变为适当的值。
	#endif
	*/
	// this is only for Windows 95 (or higher)
	//VERIFY(m_bEnabled = ( GetVersion() & 0xff ) >= 4);
	//if (!m_bEnabled) return FALSE;
	m_bEnabled = TRUE;

	//Make sure Notification window is valid
	/*VERIFY(m_bEnabled = (pWnd && ::IsWindow(pWnd->GetSafeHwnd())));
	if (!m_bEnabled) return FALSE;*/
	
	//Make sure we avoid conf->ict with other messages
	ASSERT(uCallbackMessage >= WM_USER);

	//Tray only supports tooltip text up to 64 characters
	ASSERT(_tcslen(szToolTip) <= 64);

	// load up the NOTIFYICONDATA structure
	m_bNotify = bIsNotify;

	m_tnd.cbSize = sizeof( NOTIFYICONDATA );
	m_tnd.hWnd	 = hWnd;
	m_tnd.uID	 = uID;
	m_tnd.hIcon  = icon;
	m_tnd.uCallbackMessage = uCallbackMessage;

	if (m_bNotify)
	{
		m_tnd.uVersion = NOTIFYICON_VERSION;
		m_tnd.dwInfoFlags = NIIF_INFO;
		m_tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		/*CString WindowTitle;
		pWnd->GetWindowText(WindowTitle);*/
		lstrcpy(m_tnd.szInfo, szToolTip);
		lstrcpy(m_tnd.szInfoTitle, szWindowTitle);
		lstrcpy(m_tnd.szTip, szWindowTitle);
	}
	else
	{
		m_tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		lstrcpy (m_tnd.szTip, szToolTip);
	}
	m_hWnd = hWnd;
	m_MenuID = uID;

	// Set the tray icon
	VERIFY(m_bEnabled = Ta_Shell_NotifyIcon(NIM_ADD, &m_tnd));
	Ta_Shell_NotifyIcon(NIM_SETVERSION, &m_tnd);


// 	m_tnd.cbSize = sizeof(NOTIFYICONDATA);
// 	m_tnd.hWnd	 = pWnd->GetSafeHwnd();
// 	m_tnd.uID	 = uID;
// 	m_tnd.hIcon  = icon;
// 	m_tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
// 	m_tnd.uCallbackMessage = uCallbackMessage;
// 	strcpy (m_tnd.szTip, szToolTip);
// 	VERIFY(m_bEnabled = Ta_Shell_NotifyIcon(NIM_ADD, &m_tnd));

	return m_bEnabled;
}

CTrayIcon::~CTrayIcon()
{
	m_menu_ptr = NULL;
	RemoveIcon();
}


/////////////////////////////////////////////////////////////////////////////
// CTrayIcon icon manipulation

void CTrayIcon::MoveToRight()
{
	HideIcon();
	ShowIcon();
}

void CTrayIcon::RemoveIcon()
{
	if (!m_bEnabled) return;

	m_tnd.uFlags = 0;
    Ta_Shell_NotifyIcon(NIM_DELETE, &m_tnd);
    m_bEnabled = FALSE;
}

void CTrayIcon::HideIcon()
{
	if (m_bEnabled && !m_bHidden) {
		m_tnd.uFlags = NIF_ICON;
		Ta_Shell_NotifyIcon (NIM_DELETE, &m_tnd);
		m_bHidden = TRUE;
	}
}

void CTrayIcon::ShowIcon()
{
	if (m_bEnabled && m_bHidden) {
		m_tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		Ta_Shell_NotifyIcon(NIM_ADD, &m_tnd);
		m_bHidden = FALSE;
	}
}

BOOL CTrayIcon::SetIcon(HICON hIcon)
{
	if (!m_bEnabled) return FALSE;

	m_tnd.uFlags = NIF_ICON;
	m_tnd.hIcon = hIcon;

	return Ta_Shell_NotifyIcon(NIM_MODIFY, &m_tnd);
}

BOOL CTrayIcon::SetIcon(LPCTSTR lpszIconName)
{
	HICON hIcon = LoadIconW(global::hInstance, lpszIconName);
	
	return SetIcon(hIcon);
}

/*BOOL CTrayIcon::SetIcon(UINT nIDResource)
{
	HICON hIcon = AfxGetApp()->LoadIcon(nIDResource);

	return SetIcon(hIcon);
}*/

BOOL CTrayIcon::SetStandardIcon(LPCTSTR lpIconName)
{
	HICON hIcon = LoadIcon(NULL, lpIconName);

	return SetIcon(hIcon);
}

BOOL CTrayIcon::SetStandardIcon(UINT nIDResource)
{
	HICON hIcon = LoadIcon(NULL, MAKEINTRESOURCE(nIDResource));

	return SetIcon(hIcon);
}

HICON CTrayIcon::GetIcon() const
{
	HICON hIcon = NULL;
	if (m_bEnabled)
		hIcon = m_tnd.hIcon;

	return hIcon;
}

/////////////////////////////////////////////////////////////////////////////
// CTrayIcon tooltip text manipulation

BOOL CTrayIcon::SetTooltipText(LPCTSTR pszTip, DWORD uTimeOut)
{
	if (!m_bEnabled) return FALSE;
	if (pszTip == NULL && m_bNotify)
	{
		m_uLastTimeOut = 0;
		memset(m_tnd.szInfo,0,sizeof(m_tnd.szInfo));

		// m_hTimerThread = NULL;
		m_hTimer = NULL;

		Ta_Shell_NotifyIcon(NIM_MODIFY, &m_tnd);
		return TRUE;
	}

	if (m_bNotify)
	{
		m_tnd.uFlags = NIF_INFO;
		lstrcpy(m_tnd.szInfo, pszTip);
		m_tnd.dwInfoFlags = NIIF_INFO;
		m_tnd.uTimeout = uTimeOut;
		m_tnd.uVersion = NOTIFYICON_VERSION;
		m_uLastTimeOut = uTimeOut;
		/*if (m_hTimerThread != NULL)
		{
			TerminateThread(m_hTimerThread,0);	
 		}
		m_hTimerThread = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TimerThread, (LPVOID)this, 0, NULL);
		*/
		/*if (m_hTimer != NULL) {
			KillTimer(m_hWnd, m_hTimer);
		}
		m_hTimer = SetTimer(m_hWnd, TRAY_TIMER_ID, uTimeOut, TimerProc);*/
	}
	else
	{
		m_tnd.uFlags = NIF_TIP;
		lstrcpy(m_tnd.szTip, pszTip);
	}

	return Ta_Shell_NotifyIcon(NIM_MODIFY, &m_tnd);
}

void CALLBACK CTrayIcon::TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	return;
}

/*BOOL CTrayIcon::SetTooltipText(UINT nID,DWORD uTimeOut)
{
	CString strText;
	VERIFY(strText.LoadString(nID));

	return SetTooltipText(strText,uTimeOut);
}*/

DWORD WINAPI CTrayIcon::TimerThread(LPVOID lparam)
{
	Sleep(((CTrayIcon *)lparam)->m_uLastTimeOut);
	((CTrayIcon *)lparam)->SetTooltipText((LPCTSTR)NULL,0);
	return 0;
}

void CTrayIcon::GetTooltipText(std::wstring& text)
{
	if (m_bEnabled)
		text = m_tnd.szTip;
}

/////////////////////////////////////////////////////////////////////////////
// CTrayIcon notification window stuff

BOOL CTrayIcon::SetNotificationWnd(HWND hWnd)
{
	if (!m_bEnabled) return FALSE;

	//Make sure Notification window is valid
	//ASSERT(pWnd && ::IsWindow(pWnd->GetSafeHwnd()));

	m_tnd.hWnd = hWnd;
	m_tnd.uFlags = 0;

	return Ta_Shell_NotifyIcon(NIM_MODIFY, &m_tnd);
}

HWND CTrayIcon::GetNotificationWnd() const
{
	return m_tnd.hWnd;
}

/////////////////////////////////////////////////////////////////////////////
// CTrayIcon implentation of OnTrayNotification

LRESULT CTrayIcon::OnTrayNotification(UINT wParam, LONG lParam) 
{
	//Return quickly if its not for this tray icon
	if (wParam != m_tnd.uID)
		return 0L;

	//CMenu menu, *pSubMenu;
	if (!m_menu_ptr) {
		return 0;
	}

	// Clicking with right button brings up a context menu
	switch (LOWORD(lParam))
	{
	case WM_RBUTTONUP:
	{
		//if (!menu.LoadMenu(m_tnd.uID)) return 0;
		//if (!(pSubMenu = menu.GetSubMenu(0))) return 0;
		// Make first menu item the default (bold font)
		::SetMenuDefaultItem(m_menu_ptr->GetMenu(), 0, TRUE);

		m_menu_ptr->Show(m_tnd.hWnd);
		//menu.DestroyMenu();
	}
	break;
	/*case WM_LBUTTONDBLCLK:
	{
		if (!menu.LoadMenu(m_tnd.uID)) return 0;
		if (!(pSubMenu = menu.GetSubMenu(0))) return 0;

		// double click received, the default action is to execute first menu item
		::SetForegroundWindow(m_tnd.hWnd);
		::SendMessage(m_tnd.hWnd, WM_COMMAND, pSubMenu->GetMenuItemID(0), 0);
		menu.DestroyMenu();
	}
	break;*/
	}

	return 1;
}