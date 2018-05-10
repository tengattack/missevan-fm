#include "StdAfx.h"
#include "MainTray.h"

#include "base/global.h"
#include "ui/MainWindow.h"
#include "ui/Menu.h"
#include "Server.h"
#include "UserAccount.h"
#include "ChatManager.h"
#include "LivePublisher.h"
#include "MissEvanFMWindow.h"

enum TrayMenuType {
	kTMTUser = 201,
	kTMTPush,
	kTMTPull,
	kTMTUpdate,
	kTMTAbout,
	kTMTExit,
};

enum TrayPushMenuType {
	kTMTState = 301,
	kTMTAudioMic,
	kTMTAudioHook,
	kTMTAudioCopyMicLeftChannel,
};


CMainTray::CMainTray()
	: m_mainMenu(NULL)
	, m_pushMenu(NULL)
	, m_server_ptr(NULL)
{
}


CMainTray::~CMainTray()
{
	if (m_mainMenu) {
		delete m_mainMenu;
	}
	if (m_pushMenu) {
		delete m_pushMenu;
	}
}

BOOL CMainTray::Create(HWND hWnd, UINT uCallbackMessage, LPCTSTR szTip, HICON icon, UINT uID, BOOL bIsNotify, LPCTSTR szWindowTitle)
{
	int ret = CTrayIcon::Create(hWnd, uCallbackMessage, szTip, icon, uID, bIsNotify, szWindowTitle);

	if (ret) {
		m_pushMenu = new CMenu;
		m_mainMenu = new CMenu;
		m_pushMenu->CreatePopup();

		m_pushMenu->Append(kTMTState, L"������");
		m_pushMenu->Append(kTMTAudioMic, L"����˷�");
		m_pushMenu->Append(kTMTAudioHook, L"���ñ�����");
		m_pushMenu->Append(kTMTAudioCopyMicLeftChannel, L"�޸���˷�������");

		m_pushMenu->EnableItem(kTMTState, false, false);
		m_pushMenu->EnableItem(kTMTAudioMic, false, false);
		m_pushMenu->EnableItem(kTMTAudioHook, false, false);
		m_pushMenu->EnableItem(kTMTAudioCopyMicLeftChannel, false, false);

		m_mainMenu->CreatePopup();

		m_mainMenu->Append(kTMTUser, L"�û���δ��¼��");
		m_mainMenu->Append(kTMTPush, L"����", MF_STRING, m_pushMenu);
		m_mainMenu->Append(kTMTPull, L"�����������У�");
		m_mainMenu->Append(kTMTUpdate, L"������");
		m_mainMenu->Append(kTMTAbout, L"����");
		m_mainMenu->Append(kTMTExit, L"�˳�");

		//m_mainMenu->EnableItem(kTMTUser, false, false);
		m_mainMenu->EnableItem(kTMTPull, false, false);

		SetMenu(m_mainMenu);
	}

	return ret;
}

void CMainTray::SetServer(Server *p_server)
{
	m_server_ptr = p_server;
}

void CMainTray::UpdateMenu()
{
	if (m_server_ptr) {
		//m_server_ptr->
		//kStatUser;
		//kStatChat;
		//kStatPlayer
		uint32_t stat = m_server_ptr->GetStat();
		if (stat & Server::kStatUser) {
			std::wstring userStr = L"�û���";
			userStr += m_server_ptr->m_user_ptr->GetUsername();
			userStr += L"��";
			m_mainMenu->Modify(kTMTUser, userStr.c_str());
		} else {
			m_mainMenu->Modify(kTMTUser, L"�û���δ��¼��");
		}
		if (stat & Server::kStatPushConnect || stat & Server::kStatPushLive) {
			m_pushMenu->Modify(kTMTState, L"������");
			if (stat & Server::kStatPushLive) {
				bool enabled = m_server_ptr->m_publisher_ptr->IsLoopbackEnabled();
				m_pushMenu->Modify(kTMTAudioMic, L"�ر���˷�");
				if (enabled) {
					m_pushMenu->Modify(kTMTAudioHook, L"�رձ�����");
				} else {
					m_pushMenu->Modify(kTMTAudioHook, L"���ñ�����");
				}
				enabled = m_server_ptr->m_publisher_ptr->IsCopyMicLeftChannel();
				m_pushMenu->CheckItem(kTMTAudioCopyMicLeftChannel, enabled, false);
				m_pushMenu->EnableItem(kTMTAudioMic, false, false);
				m_pushMenu->EnableItem(kTMTAudioCopyMicLeftChannel, true, false);
			} else {
				if (m_server_ptr->m_cm_ptr->IsMicOpened()) {
					m_pushMenu->Modify(kTMTAudioMic, L"�ر���˷�");
				}
				else {
					m_pushMenu->Modify(kTMTAudioMic, L"����˷�");
				}
				if (m_server_ptr->m_cm_ptr->IsBGMEnabled()) {
					m_pushMenu->Modify(kTMTAudioHook, L"�رձ�����");
				} else {
					m_pushMenu->Modify(kTMTAudioHook, L"���ñ�����");
				}
				m_pushMenu->CheckItem(kTMTAudioCopyMicLeftChannel, false, false);
				m_pushMenu->EnableItem(kTMTAudioMic, true, false);
				m_pushMenu->EnableItem(kTMTAudioCopyMicLeftChannel, false, false);
			}
			m_pushMenu->EnableItem(kTMTAudioHook, true, false);
		} else {
			m_pushMenu->Modify(kTMTState, L"������");
			m_pushMenu->Modify(kTMTAudioMic, L"����˷�");
			m_pushMenu->Modify(kTMTAudioHook, L"���ñ�����");
			m_pushMenu->EnableItem(kTMTAudioMic, false, false);
			m_pushMenu->EnableItem(kTMTAudioHook, false, false);
		}
		m_pushMenu->EnableItem(kTMTState, false, false);
		if (stat & Server::kStatPlayer) {
			m_mainMenu->Modify(kTMTPull, L"������");
			m_mainMenu->EnableItem(kTMTPull, true, false);
		} else {
			m_mainMenu->Modify(kTMTPull, L"�����������У�");
			m_mainMenu->EnableItem(kTMTPull, false, false);
		}
	}
}

LRESULT CMainTray::OnTrayNotification(WPARAM uID, LPARAM lEvent)
{
	switch (LOWORD(lEvent)) {
	case NIN_BALLOONUSERCLICK:
		ShellExecute(0, 0, L"https://fm.missevan.com", 0, 0, SW_SHOW);
		return 1;
	case WM_RBUTTONUP:
		UpdateMenu();
		break;
	}
	return CTrayIcon::OnTrayNotification(uID, lEvent);
}

LRESULT CMainTray::OnTrayMenu(UINT wParam, LONG lParam)
{
	switch (wParam) {
	case kTMTAudioMic: {
		uint32_t stat = m_server_ptr->GetStat();
		if (stat & Server::kStatPushConnect) {
			if (m_server_ptr->m_cm_ptr->IsMicOpened()) {
				m_server_ptr->m_cm_ptr->CloseMic();
			} else {
				m_server_ptr->m_cm_ptr->OpenMic();
			}
		}
	}
	break;
	case kTMTAudioHook: {
		// check live mode
		uint32_t stat = m_server_ptr->GetStat();
		if (stat & Server::kStatPushLive) {
			bool enabled = m_server_ptr->m_publisher_ptr->IsLoopbackEnabled();
			if (!enabled) {
				MessageBox(m_hWnd, L"����ģʽ�£������򽫻��ϵͳ���ŵ�����һ��ֱ����", L"��ʾ", MB_ICONINFORMATION | MB_OK);
			}
			if (!m_server_ptr->m_publisher_ptr->EnableLookbackCapture(!enabled)) {
				MessageBox(m_hWnd, L"�޷�����ϵͳ������¼��", L"����", MB_ICONERROR | MB_OK);
			}
		} else if (stat & Server::kStatPushConnect) {
			bool enabled = m_server_ptr->m_cm_ptr->IsBGMEnabled();
			if (!m_server_ptr->m_cm_ptr->EnableBGM(!enabled, m_hWnd)) {
				MessageBox(m_hWnd, L"�޷�����ϵͳ������¼��", L"����", MB_ICONERROR | MB_OK);
			}
		}
	}
	break;
	case kTMTAudioCopyMicLeftChannel: {
		bool enabled = m_server_ptr->m_publisher_ptr->IsCopyMicLeftChannel();
		m_server_ptr->m_publisher_ptr->EnableCopyMicLeftChannel(!enabled);
	}
	break;
	case kTMTUpdate: {
		((CMissEvanFMWindow *)global::mainWindow)->InitUpdate(false);
		break;
	}
	case kTMTAbout: {
		global::mainWindow->Show();
	}
	break;
	case kTMTExit: {
		// close main window, mark can close
		PostMessage(m_hWnd, WM_CLOSE, 1, 0);
	}
	break;
	default: {
		return 0;
	}
	}
	return 1;
}
