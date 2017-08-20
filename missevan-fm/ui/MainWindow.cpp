#include "stdafx.h"

#include <string>

#include "MainWindow.h"
#include "TrayIcon.h"

#include "../base/common.h"
#include "../base/global.h"
#include "../resource.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifndef SCALING_ENUMS_DECLARED
typedef enum _PROCESS_DPI_AWARENESS {
	PROCESS_DPI_UNAWARE = 0,
	PROCESS_SYSTEM_DPI_AWARE = 1,
	PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
#endif
typedef HRESULT (WINAPI * fn_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS value);
#ifdef __cplusplus
}
#endif

std::wstring CMainWindow::main_window_class;

CMainWindow::CMainWindow()
	: m_hWnd(NULL)
	, m_trayIcon_ptr(NULL)
{
	m_hIcon = (HICON)LoadImage(global::hInstance, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	m_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

CMainWindow::~CMainWindow()
{
	if (m_hWnd) {
		CloseWindow(m_hWnd);
	}
	if (m_hIcon) {
		DestroyIcon(m_hIcon);
	}
	m_trayIcon_ptr = NULL;
}

bool CMainWindow::Init(LPCTSTR szClassName)
{
	// hidpi
	HMODULE hDllShCore = LoadLibrary(L"shcore.dll");
	if (hDllShCore) {
		fn_SetProcessDpiAwareness spda = (fn_SetProcessDpiAwareness)GetProcAddress(hDllShCore, "SetProcessDpiAwareness");
		if (spda != NULL) {
			spda(PROCESS_PER_MONITOR_DPI_AWARE);
		}
	}

	WNDCLASSEXW wcex;

	main_window_class = szClassName;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = global::hInstance;
	wcex.hIcon = global::hIcon;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szClassName;
	wcex.hIconSm = global::hIcon;
	//wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex) != NULL;
}

LRESULT CALLBACK CMainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT ret = 0;
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// ·ÖÎö²Ëµ¥Ñ¡Ôñ: 
		/*switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}*/
	}
	break;
	case WM_CREATE:
	{
		if (global::mainWindow) {
			ret = global::mainWindow->OnCreate(hWnd);
		}
	}
	break;
	case WM_USER_TRAYICON:
	{
		if (global::mainWindow && global::mainWindow->m_trayIcon_ptr) {
			ret = global::mainWindow->m_trayIcon_ptr->OnTrayNotification(wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		if (global::mainWindow) {
			ret = global::mainWindow->OnPaint();
		}
	}
	break;
	case WM_TIMER:
	{
		if (global::mainWindow) {
			ret = global::mainWindow->OnTimer(wParam, (TIMERPROC)lParam);
		}
	}
	break;
	case WM_CLOSE:
		if (wParam == 0) {
			ShowWindow(hWnd, SW_HIDE);
			return 1;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 1;
	}
	if (ret == 0) {
		ret = DefWindowProc(hWnd, message, wParam, lParam);
	}
	return ret;
}

LRESULT CALLBACK CMainWindow::OnCreate(HWND hWnd)
{
	m_hWnd = hWnd;
	return 1;
}

LRESULT CALLBACK CMainWindow::OnPaint()
{
	PAINTSTRUCT ps;

	HDC hdc = BeginPaint(m_hWnd, &ps);
	SelectObject(hdc, m_hFont);
	EndPaint(m_hWnd, &ps);

	return 1;
}

LRESULT CALLBACK CMainWindow::OnTimer(UINT_PTR nIDEvent, TIMERPROC fnTimerProc)
{
	return 0;
}

bool CMainWindow::Create(LPCTSTR szTitle, bool bShow)
{
	return Create(szTitle, 0, 0, bShow);
}

bool CMainWindow::Create(LPCTSTR szTitle, int width, int height, bool bShow)
{
	global::mainWindow = this;
	HWND hWnd = CreateWindow(main_window_class.c_str(), szTitle,
		(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX),
		CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, global::hInstance, nullptr);

	if (!hWnd)
	{
		global::mainWindow = NULL;
		return false;
	}

	ShowWindow(hWnd, bShow ? SW_SHOW : SW_HIDE);
	UpdateWindow(hWnd);

	return true;
}

bool CMainWindow::Show(bool bShow)
{
	return ShowWindow(m_hWnd, bShow ? SW_SHOW : SW_HIDE) != FALSE;
}

UINT_PTR CMainWindow::SetTimer(UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc)
{
	return ::SetTimer(m_hWnd, nIDEvent, uElapse, lpTimerFunc);
}

bool CMainWindow::KillTimer(UINT_PTR nIDEvent)
{
	return static_cast<bool>(::KillTimer(m_hWnd, nIDEvent));
}