// missevan-fm.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include "missevan-fm.h"

#include <base/defer_ptr.h>

#include "base/common.h"
#include "base/global.h"
#include "Server.h"
#include "DeviceManager.h"
#include "ChatManager.h"
#include "UserAccount.h"
#include "MissEvanFMWindow.h"
#include "MainTray.h"

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
		trayIcon.SetTooltipText(L"猫耳FM直播助手正在后台运行，请在浏览器中进行直播~", 15000);
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
	DEFER_INIT();
	if (!global::Init(hInstance)) {
		return 1;
	}
	DEFER(global::Uninit());

	CMainWindow::Init(APP_CLASSNAME);

	DeviceManager::Init();
	DEFER(DeviceManager::Cleanup());

	bool ret = UserAccount::Init();
	bool ret_vchat = false;
	printf("nim_client_init: %d\n", ret ? 1 : 0);

	if (ret) {
		DEFER(UserAccount::Cleanup());

		ret_vchat = ChatManager::Init();
		printf("nim_vchat_init: %d\n", ret ? 1 : 0);

		if (ret_vchat) {
			DEFER(ChatManager::Cleanup());
			printf("init done.\n");
		} else {
			printf("nim_vchat_init failed!\n");
			return 1;
		}
	} else {
		printf("nim_client_init failed!\n");
		return 1;
	}

	Server server;
	HANDLE hThread = CreateThread(NULL, NULL, UIThreadProc, &server, NULL, NULL);
	
	if (!server.start()) {
		printf("start server failed\n");
	}

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	
	//scanf("%*c");
	printf("cleaning up...\n");
	UserAccount::GetInstance()->Logout();
	
    return 0;
}

