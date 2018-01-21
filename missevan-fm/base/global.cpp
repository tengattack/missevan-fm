
#include "stdafx.h"

#include <Windows.h>
#include <shlobj.h>

#include "global.h"
#include "common.h"
#include "config.h"
#include "../resource.h"

#include "../ui/MainWindow.h"

namespace global {
	HINSTANCE hInstance = NULL;
	HICON hIcon = NULL;

	std::wstring wpath;
	std::string apath;
	std::wstring log_path;

	CMainWindow *mainWindow = NULL;

	void InitPath()
	{
		wchar_t szwpath[MAX_PATH];
		char szapath[MAX_PATH];

		wchar_t szlogpath[MAX_PATH];
		log_path.clear();
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szlogpath)))
		{
			log_path = szlogpath;
			log_path += L"\\";
		}
		log_path += _T(APP_LOG_FOLDER);
		CreateDirectory(log_path.c_str(), NULL);
		log_path += L"\\";

		GetModuleFileNameW(NULL, szwpath, MAX_PATH);
		for (int i = lstrlen(szwpath) - 1; i > 0; i--)
		{
			if (szwpath[i] == '\\')
			{
				szwpath[i + 1] = 0;
				break;
			}
		}

		GetModuleFileNameA(NULL, szapath, MAX_PATH);
		for (int i = lstrlenA(szapath) - 1; i > 0; i--)
		{
			if (szapath[i] == '\\')
			{
				szapath[i + 1] = 0;
				break;
			}
		}

		wpath = szwpath;
		apath = szapath;
	}

	bool Init(HINSTANCE _hInstance) {
		hInstance = _hInstance;

		HRESULT hr = ::OleInitialize(NULL);
		if (FAILED(hr)) {
			printf("old init failed.\n");
			return false;
		}

		CoInitializeEx(NULL, COINIT_MULTITHREADED);

		InitPath();
		hIcon = LoadIcon(_hInstance, MAKEINTRESOURCE(IDI_MAIN));

		// read config file
		config::read();

		return true;
	}

	void Uninit() {
		::CoUninitialize();
		::OleUninitialize();
	}

	bool NetInit() {
		WORD version;
		WSADATA wsaData;
		version = MAKEWORD(2, 2);
		return (WSAStartup(version, &wsaData) == 0);
	}

	void NetUninit() {
		WSACleanup();
	}
}