
#ifndef _MFM_GLOBAL_H_
#define _MFM_GLOBAL_H_

#pragma once

#include <string>

class CMainWindow;

namespace global {
	extern HINSTANCE hInstance;
	extern HICON hIcon;

	extern bool Init(HINSTANCE hInstance);
	extern void Uninit();

	extern bool NetInit();
	extern void NetUninit();

	extern std::wstring wpath;
	extern std::string apath;
	extern std::wstring log_path;

	extern CMainWindow *mainWindow;
}

#endif