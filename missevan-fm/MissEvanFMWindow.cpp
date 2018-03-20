#include "stdafx.h"
#include "MissEvanFMWindow.h"

#include <curl/curl.h>
#include <base/string/string_number_conversions.h>
#include <base/string/stringprintf.h>
#include <base/json/values.h>
#include <base/json/json_reader.h>
#include <common/Buffer.h>
#include <common/strconv.h>
#include "base/common.h"
#include "base/global.h"
#include "base/UpdateInfo.h"
#include "ui/TrayIcon.h"
#include "Server.h"

#define AUTOUPDATE_TIMERID 1001

struct ProcessInfo
{
	CPBar				*pb;
	DownloadFile		*dt;
};

int my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
	struct DownloadFile *out = (struct DownloadFile *)stream;
	if (out)
	{
		if (!out->stream)
		{
			out->stream = new base::CFile;
			if (!out->stream)
			{
				return -1;
			}
			if (!out->stream->Open(base::kFileCreate, out->filename))
			{
				delete out->stream;
				out->stream = NULL;
				return -1;
			}
		}
		if (out->stopped)
		{
			if (*out->stopped)
			{
				return -1;
			}
		}
		if (out->stream->Write((unsigned char *)buffer, nmemb * size))
		{
			return nmemb;
		}
	}

	return -1;
}

//进度条显示函数 
int curldav_dl_progress_func(void* ptr, double rDlTotal, double rDlNow, double rUlTotal, double rUlNow)
{
	ProcessInfo* pi = (ProcessInfo *)ptr;
	if (pi)
	{
		pi->pb->SetPos(100.0f * float(CMissEvanFMWindow::m_base_size + pi->dt->currentpos + rDlNow) / (float)pi->dt->totalsize);	//设置进度条的值 
	}
	return 0;
}

int CMissEvanFMWindow::m_base_size = 0;


CMissEvanFMWindow::CMissEvanFMWindow()
	: CMainWindow()
	, m_ui(NULL)
	, m_stopping(false)
	, m_downloading(false)
	, m_succeed(kUTError)
	, m_autoupdate(true)
{
}

CMissEvanFMWindow::~CMissEvanFMWindow()
{
	if (m_ui) delete m_ui;
}

void CMissEvanFMWindow::SwitchText(int iText)
{
}

LRESULT CALLBACK CMissEvanFMWindow::OnCreate(HWND hWnd)
{
	CMainWindow::OnCreate(hWnd);
	SetTimer(AUTOUPDATE_TIMERID, 20000, NULL);
	return 1;
}

LRESULT CALLBACK CMissEvanFMWindow::OnPaint()
{
	static const TCHAR* szVersion = _T("猫耳FM直播助手\r\n版本: v" APP_VERSION);

	PAINTSTRUCT ps;

	HDC hdc = BeginPaint(m_hWnd, &ps);
	//HBRUSH hbr = (HBRUSH)GetStockObject(DC_BRUSH);

	SelectObject(hdc, m_hFont);
	//SelectObject(hdc, hbr);

	//DrawIconEx(hdc, 206, 94, m_hIcon, 36, 36, NULL, hbr, DI_IMAGE);
	DrawIcon(hdc, 206, 94, m_hIcon);

	RECT rc = { 256, 100, 500, 300 };
	DrawText(hdc, szVersion, _tcslen(szVersion), &rc, DT_LEFT);

	EndPaint(m_hWnd, &ps);
	return 1;
}

LRESULT CALLBACK CMissEvanFMWindow::OnTimer(UINT_PTR nIDEvent, TIMERPROC fnTimerProc)
{
	switch (nIDEvent) {
	case AUTOUPDATE_TIMERID:
		KillTimer(AUTOUPDATE_TIMERID);
		InitUpdate();
		return 1;
	}
	return 0;
}

void CMissEvanFMWindow::InitUpdate(bool autoupdate)
{
#ifdef _DEBUG
	if (autoupdate) {
		return;
	}
#endif
	//listview.Clear();
	if (m_downloading) {
		return;
	}

	m_autoupdate = autoupdate;
	m_stopping = false;
	SetSucceed(kUTError);

	//TAMSGBOXINFO_ID(m_hWnd, TA_BASE_W_TEXT_UPDATE_TIPS);

	//work
	::CloseHandle(
		::CreateThread(NULL, 0, InitUpdateProc, this, NULL, NULL)
	);
}

void CMissEvanFMWindow::FinishUpdate(CMissEvanFMWindow* pWindow, CUpdateInfo* ui)
{
	if (!pWindow) return;

	pWindow->m_downloading = false;
	if (pWindow->m_ui) {
		delete pWindow->m_ui;
	}
	pWindow->m_ui = ui;

	if (ui == NULL) {
		return;
	}
	//pWindow->m_users.Clear();

	switch (pWindow->GetSucceed())
	{
	case kUTSucceed:
		if (ui->HasRun())
		{
			//pWindow->button[0].SetText(L"运行");
			//pWindow->button[0].Enable();
			// Run
			ui->Run();
		} else {
			//pWindow->button[0].SetText(L"更新完成");
			//pWindow->button[0].Enable(false);
		}
		if (pWindow->m_trayIcon_ptr) {
			pWindow->m_trayIcon_ptr->SetTooltipText(_T("更新成功，重启程序将会生效~"));
		}
		break;
	case kUTError:
		//pWindow->button[0].SetText(L"更新");
		//pWindow->button[0].Enable();
		if (pWindow->m_trayIcon_ptr) {
			pWindow->m_trayIcon_ptr->SetTooltipText(_T("更新出错！"));
		}
		break;
	}
	//pWindow->button[1].Enable();

	return;
}

std::string DownloadJsonStr(LPCSTR url)
{
	std::string json;

	//创建curl对象
	CURL *curl;
	CURLcode res;

	//curl初始化
	curl = curl_easy_init();
	//curl对象存在的情况下执行操作
	if (curl)
	{
		CBuffer bufdata;
		//设置远端地址 
		curl_easy_setopt(curl, CURLOPT_URL, url);
		//执行写入文件流操作 
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite_tomem);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bufdata);

		//写入文件
		res = curl_easy_perform(curl);
		//释放curl对象 
		curl_easy_cleanup(curl);

		if (CURLE_OK == res)
		{
			json = std::string((const char *)bufdata.GetBuffer(), bufdata.GetBufferLen());
		}
	}

	return json;
}

DWORD WINAPI CMissEvanFMWindow::InitUpdateProc(LPVOID lParam)
{
	CMissEvanFMWindow *pWindow = (CMissEvanFMWindow *)lParam;
	if (!pWindow) return 0;

	pWindow->m_downloading = true;

	wchar_t path[MAX_PATH], filepath[MAX_PATH], tmppath[MAX_PATH];
	lstrcpy(path, global::wpath.c_str());
	lstrcpy(tmppath, path);
	lstrcat(tmppath, TEMP_FOLDER);
	::CreateDirectory(tmppath, NULL);

	//获取更新信息
	bool get_download_url = false;
	std::string json = DownloadJsonStr(APP_UPDATE_URL);

	if (!json.empty()) {
		Value* v = base::JSONReader::Read(json.c_str(), true);
		if (v)
		{
			if (v->GetType() == Value::TYPE_DICTIONARY) {
				DictionaryValue* dv = (DictionaryValue *)v;
				std::string state;
				dv->GetString("state", &state);
				if (state == "success") {
					std::string download_url;
					dv->GetDictionary("info", &dv);
					if (dv) {
						dv->GetString("download", &download_url);
						if (!download_url.empty()) {
							get_download_url = true;
							json = DownloadJsonStr(download_url.c_str());
						}
					}
				}
			}
			delete v;
		}
	}

	if (!get_download_url || json.empty()) {
		pWindow->SwitchText(8);
		FinishUpdate(pWindow, NULL);
		return 0;
	}

	CUpdateInfo* ui = new CUpdateInfo(path);
	// delete in ~CMissEvanFMWindow
	//std::auto_ptr<CUpdateInfo> ptr_ui(ui);

	if (ui->Load(json.c_str()))
	{
		if (ui->HasTips())
		{
			MessageBox(pWindow->hWnd(), ui->GetTips(), L"提示", MB_ICONQUESTION);
		} else {
			if (!pWindow->m_autoupdate) {
				std::string& version = ui->GetVersion();
				CC2W wstr(version.c_str());
				std::wstring msg = base::StringPrintf(L"开始下载更新文件 (v%s)~", wstr.c_str());
				pWindow->m_trayIcon_ptr->SetTooltipText(msg.c_str());
			}
		}
		if (!ui->NeedToUpdate())
		{
			pWindow->SetSucceed(kUTLatest);
			if (pWindow->m_autoupdate) {
				pWindow->SwitchText(7);
				FinishUpdate(pWindow, ui);
				return 0;
			} else {
				/*if (MessageBox(pWindow->hWnd(),
					L"版本检查发现当前版本已经是最新版本，是否检查文件完整性？",
					L"提示", MB_YESNO | MB_ICONQUESTION) != IDYES)
				{
					pWindow->SwitchText(7);
					FinishUpdate(pWindow, ui);
					return 0;
				}*/
				pWindow->m_trayIcon_ptr->SetTooltipText(_T("当前版本已经是最新版本~"));
				pWindow->SwitchText(7);
				FinishUpdate(pWindow, ui);
				return 0;
			}
		}
	} else {
		pWindow->SwitchText(8);
		FinishUpdate(pWindow, ui);
		return 0;
	}

	pWindow->SwitchText(2);

	bool haserror = true;
	int totalsize = ui->GetTotalSize();
	if (totalsize == 0)
	{
		pWindow->SwitchText(8);
		FinishUpdate(pWindow, ui);
		return 0;
	}

	m_base_size = int(float(totalsize) * 0.03f);	//占 3%

	struct DownloadFile dfile =
	{
		//定义下载到本地的文件位置和路径 
		NULL, &pWindow->m_stopping, NULL, 0, totalsize + m_base_size * 2
	};

	struct ProcessInfo pi = {
		&pWindow->pb, &dfile
	};

	pWindow->pb.SetPos(100.0f * float(m_base_size) / float(dfile.totalsize));

	int iFile;
	for (iFile = 0; iFile < ui->GetFilesCount(); iFile++)
	{
		pWindow->listview.InsertItem(iFile, CUpdateInfo::GetUrlFileName(ui->GetFile(iFile).url.c_str()), 0);
		pWindow->listview.SetItemText(iFile, 1, base::IntToString16(ui->GetFile(iFile).size).c_str());
		pWindow->listview.SetItemText(iFile, 2, ui->GetFile(iFile).path.c_str());
	}

	for (iFile = 0; iFile < ui->GetFilesCount(); iFile++)
	{
		pWindow->listview.EnsureVisible(iFile, true);

		lstrcpy(filepath, tmppath);
		lstrcat(filepath, L"\\");

		LPCWSTR filename = CUpdateInfo::GetUrlFileName(ui->GetFile(iFile).url.c_str());
		lstrcat(filepath, filename);

		dfile.filename = filepath;

		haserror = true;

		if (ui->NeedToDownload(iFile))
		{
			//设置下载图标
			pWindow->listview.SetItemImage(iFile, 1);

			//创建curl对象
			CURL *curl;
			CURLcode res;

			//curl初始化 
			curl = curl_easy_init();
			//curl对象存在的情况下执行操作
			if (curl)
			{
				char* url = NULL;
				lo_W2C(&url, ui->GetFile(iFile).url.c_str());

				//设置远端地址 
				curl_easy_setopt(curl, CURLOPT_URL, url);
				//执行写入文件流操作 
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_fwrite);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dfile);

				curl_easy_setopt(curl, CURLOPT_USERAGENT, "SnowUpdate/1.0");

				//curl的进度条声明 
				curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
				//回调进度条函数 
				curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curldav_dl_progress_func);
				//设置进度条名称 
				curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &pi);

				//写入文件
				res = curl_easy_perform(curl);
				//释放curl对象 
				curl_easy_cleanup(curl);

				free(url);
				if (CURLE_OK == res)
				{
					haserror = false;
				}
			}
			if (dfile.stream)
			{
				//关闭文件流
				dfile.stream->Close();
				delete dfile.stream;
				dfile.stream = NULL;
			}

			//校验文件
			if (ui->CheckFile(iFile, filepath))
			{
				pWindow->listview.SetItemImage(iFile, 2);
			} else {
				pWindow->listview.SetItemImage(iFile, 4);
				haserror = true;
			}
		} else {
			pWindow->listview.SetItemImage(iFile, 2);
			haserror = false;
		}

		if (pWindow->m_stopping || haserror)
		{
			break;
		}

		dfile.currentpos += ui->GetFile(iFile).size;
	}

	pWindow->SwitchText(3);

	if (!pWindow->m_stopping && !haserror)
	{
		//更新文件
		for (iFile = 0; iFile < ui->GetFilesCount(); iFile++)
		{
			pWindow->listview.EnsureVisible(iFile, true);

			CUpdateInfo::UpdateStat stat = ui->UpdateFile(iFile);
			if (stat == CUpdateInfo::kUSUpdated || stat == CUpdateInfo::kUSLatest)
			{
				pWindow->listview.SetItemImage(iFile, 3);
			} else if (stat == CUpdateInfo::kUSUpdateOnReload) {
				pWindow->listview.SetItemImage(iFile, 3);
				// TODO: deal with replace list
			} else {
				pWindow->listview.SetItemImage(iFile, 4);
				haserror = true;
				break;
			}
		}

		if (!haserror)
		{
			pWindow->pb.SetPos(100);
			pWindow->SetSucceed(kUTSucceed);
			ui->SaveVersion();
		}
	}

	if (pWindow->m_stopping) {
		//停止更新
		pWindow->SwitchText(5);
	} else if (haserror) {
		//更新错误
		pWindow->SwitchText(6);
	} else {
		//成功
		ui->OpenURL();
		pWindow->SwitchText(4);
	}

	FinishUpdate(pWindow, ui);

	return 0;
}