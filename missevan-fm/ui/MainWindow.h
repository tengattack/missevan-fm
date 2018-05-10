#pragma once

class CTrayIcon;

class CMainWindow
{
public:
	CMainWindow();
	~CMainWindow();

	bool Create(LPCTSTR szTitle, bool bShow = true);
	bool Create(LPCTSTR szTitle, int width, int height, bool bShow = true);
	bool Show(bool bShow = true);
	void SetTrayIcon(CTrayIcon *trayIcon) {
		m_trayIcon_ptr = trayIcon;
	}
	inline HWND hWnd() {
		return m_hWnd;
	}

	static bool Init(LPCTSTR szClassName);

	UINT_PTR SetTimer(UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
	bool KillTimer(UINT_PTR nIDEvent);

protected:
	HWND m_hWnd;
	HICON m_hIcon;
	HFONT m_hFont;
	CTrayIcon *m_trayIcon_ptr;

	static std::wstring main_window_class;

	virtual LRESULT CALLBACK OnPaint();
	virtual LRESULT CALLBACK OnCreate(HWND hWnd);
	virtual LRESULT CALLBACK OnTimer(UINT_PTR nIDEvent, TIMERPROC fnTimerProc);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

