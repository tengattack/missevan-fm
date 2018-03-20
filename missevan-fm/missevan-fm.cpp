// missevan-fm.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"

#include "missevan-fm.h"

#include <base/defer_ptr.h>
#include <base/at_exit.h>
#include <base/logging.h>
#include <base/string/stringprintf.h>
#include <base/windows_version.h>
#include <base/debug/stack_trace.h>

#include "base/common.h"
#include "base/global.h"
#include "Server.h"
#include "DeviceManager.h"
#include "ChatManager.h"
#include "UserAccount.h"
#include "MissEvanFMWindow.h"
#include "MainTray.h"

LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter1 = NULL;

long WINAPI ExpFilter(EXCEPTION_POINTERS* info)
{
	LOG(ERROR) << base::debug::StackTrace(info).ToString();
	if (g_previous_filter1)
		return g_previous_filter1(info);
	return EXCEPTION_CONTINUE_SEARCH;
}

void EnableStackDumping()
{
	// RegisterApplicationRestart(L"", RESTART_NO_PATCH | RESTART_NO_REBOOT);
	g_previous_filter1 = SetUnhandledExceptionFilter(ExpFilter);
	base::debug::EnableInProcessStackDumping();
}

void LogSystemInfo()
{
	std::wstring strSystemInfo;
	bool bWin7OrLater = true;

	OSVERSIONINFOEX ver = {};
	ver.dwOSVersionInfoSize = sizeof(ver);

	if (GetVersionEx((OSVERSIONINFO *)&ver)) {
		bWin7OrLater = (ver.dwMajorVersion > 6) ||
			((ver.dwMajorVersion == 6) && (ver.dwMinorVersion >= 1));
		std::wstring spInfo;
		if (ver.wServicePackMajor) {
			base::SStringPrintf(&spInfo, L" SP%d", (int)ver.wServicePackMajor);
		}
		base::SStringPrintf(&strSystemInfo, L"Windows %s %u.%u%s Build: %u",
			ver.dwPlatformId == VER_PLATFORM_WIN32_NT ? L"NT" : L"Unknown", ver.dwMajorVersion, ver.dwMinorVersion,
			spInfo.c_str(), ver.dwBuildNumber);
	} else {
		strSystemInfo = L"GetVersion failed";
	}

	if (bWin7OrLater) {
		RTL_OSVERSIONINFOEXW rovi = {};
		rovi.dwOSVersionInfoSize = sizeof(rovi);
		if (base::win::GetRealOSVersion(&rovi)) {
			std::wstring spInfo;
			if (rovi.wServicePackMajor) {
				base::SStringPrintf(&spInfo, L" SP%d", (int)rovi.wServicePackMajor);
			}
			base::SStringPrintf(&strSystemInfo, L"Windows %s %u.%u%s Build: %u",
				rovi.dwPlatformId == VER_PLATFORM_WIN32_NT ? L"NT" : L"Unknown", rovi.dwMajorVersion, rovi.dwMinorVersion,
				spInfo.c_str(), rovi.dwBuildNumber);
		}
	}

	LOG(INFO) << strSystemInfo;
}

DWORD WINAPI UIThreadProc(LPVOID lpParam)
{
	Server *server_ptr = (Server *)lpParam;
	MSG msg;
	BOOL bRet;

	CMissEvanFMWindow mainWindow;
	CMainTray trayIcon;

	if (mainWindow.Create(APP_NAME, 600, 300
#ifndef _DEBUG
		, false
#endif
	)) {
		trayIcon.Create(mainWindow.hWnd(), WM_USER_TRAYICON, APP_NAME, global::hIcon, TRAYICON_ID, TRUE, APP_NAME);
		trayIcon.SetServer(server_ptr);
		mainWindow.SetTrayIcon(&trayIcon);
		trayIcon.SetTooltipText(L"è��FMֱ���������ں�̨���У�����������н���ֱ��~", 15000);
	}

	while (1) {
		bRet = GetMessage(&msg, NULL, 0, 0);

		if (bRet > 0)  // (bRet > 0 indicates a message that must be processed.)
		{
			if (msg.message == WM_COMMAND) {
				if (trayIcon.OnTrayMenu(msg.wParam, msg.lParam)) {
					continue;
				}
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else if (bRet < 0)  // (bRet == -1 indicates an error.)
		{
			// Handle or log the error; possibly exit.
			// ...
		}
		else  // (bRet == 0 indicates "exit program".)
		{
			break;
		}
	}

	server_ptr->stop();
	return 0;
}

#if defined(_MFM_DLL)
BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
)
{
	return TRUE;
}

int WINAPI MissEvanFMMain(HINSTANCE hInstance)
{
#elif defined(_DEBUG)
int main()
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
#else
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
#endif
	EnableStackDumping();
	base::AtExitManager exit_manager;
	DEFER_INIT();

	logging::LoggingSettings settings;
	settings.logging_dest = logging::LOG_TO_FILE;
	logging::InitLogging(settings);

	LOG(INFO) << "MissEvanFM v" << APP_VERSION;
	// log system info, i.e. Windows version
	LogSystemInfo();

	// check MissEvanFM is already running
	HANDLE hMutex = CreateMutex(NULL, FALSE, _T("MissEvanFM"));
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		PLOG(ERROR) << "mutex already exists";
		CloseHandle(hMutex);
		MessageBox(NULL, _T("�������Ѿ��пͻ����������ˣ�\n�����ȹر�֮ǰ�򿪵Ŀͻ��������´򿪰ɣ�"), _T("��ʾ"), MB_ICONINFORMATION | MB_OK);
		return 1;
	}

	if (!global::Init(hInstance)) {
		PLOG(ERROR) << "global init failed!";
		return 1;
	}
	DEFER(global::Uninit());

	if (!global::NetInit()) {
		PLOG(ERROR) << "network init failed!";
		return 1;
	}
	DEFER(global::NetUninit());

	CMainWindow::Init(APP_CLASSNAME);

	// DeviceManager::Init();
	// DEFER(DeviceManager::Cleanup());

	bool ret = UserAccount::Init();
	bool ret_vchat = false;
	LOG(INFO) << "nim_client_init: " << ret;

	if (ret) {
		DEFER(UserAccount::Cleanup());

		ret_vchat = ChatManager::Init();
		LOG(INFO) << "nim_vchat_init: " << ret;

		if (ret_vchat) {
			DEFER(ChatManager::Cleanup());
			LOG(INFO) << "init done.";
		} else {
			LOG(ERROR) << "nim_vchat_init failed!";
			return 1;
		}
	} else {
		LOG(ERROR) << "nim_client_init failed!";
		return 1;
	}

	Server server;
	HANDLE hThread = CreateThread(NULL, NULL, UIThreadProc, &server, NULL, NULL);

	if (!hThread) {
		PLOG(ERROR) << "create ui thread failed!";
	}
	if (!server.start()) {
		LOG(ERROR) << "start server failed!";
	}

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	
	//scanf("%*c");
	LOG(INFO) << "cleaning up...";
	UserAccount::GetInstance()->Logout();
	
    return 0;
}

