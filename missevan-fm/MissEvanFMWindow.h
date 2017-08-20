#pragma once

#include "ui/MainWindow.h"

class CUpdateInfo;

class CPBar {
public:
	CPBar() {};
	void SetPos(float pos) {};
};

class CListView {
public:
	CListView() {
	}
	bool SetItemImage(int iItem, int iImage) {
		return false;
	}
	bool EnsureVisible(int iItem, bool visible) {
		return false;
	}
	int InsertItem(int iItem, LPCWSTR lpText, int iImage) {
		return -1;
	}
	int SetItemText(int iItem, int iSubItem, LPCWSTR lpText) {
		return -1;
	}
};

class CMissEvanFMWindow : public CMainWindow
{
public:
	CMissEvanFMWindow();
	~CMissEvanFMWindow();

	enum UpdatedType {
		kUTError = -1,
		kUTSucceed = 0,
		kUTLatest,
	};

	static int m_base_size;

	void InitUpdate(bool autoupdate = true);
	void SwitchText(int iText);

	inline void SetSucceed(UpdatedType ut)
	{
		m_succeed = ut;
	}

	inline UpdatedType GetSucceed()
	{
		return m_succeed;
	}

protected:
	CUpdateInfo *m_ui;
	bool m_stopping;
	bool m_downloading;
	UpdatedType m_succeed;
	bool m_autoupdate;

	CPBar pb;
	CListView listview;

	virtual LRESULT CALLBACK OnCreate(HWND hWnd);
	virtual LRESULT CALLBACK OnPaint();
	virtual LRESULT CALLBACK OnTimer(UINT_PTR nIDEvent, TIMERPROC fnTimerProc);

	static DWORD WINAPI InitUpdateProc(LPVOID lParam);
	static void FinishUpdate(CMissEvanFMWindow* pWindow, CUpdateInfo* ui);
};

