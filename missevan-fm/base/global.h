
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

	extern std::wstring wpath;
	extern std::string apath;

	extern CMainWindow *mainWindow;
}

#endif