// MissEvanFMLoader.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include "MissEvanFMLoader.h"

#include <base/file/file.h>
#include <base/file/filedata.h>
#include <base/json/values.h>
#include <base/json/json_reader.h>

#define VERSION_PATH _T("data\\VERSION")
#define RESTART_ACTION_PATH _T("temp\\restart.json")

typedef int (WINAPI * MissEvanFMMainFunc)(HINSTANCE hInstance);
typedef BOOL (WINAPI * SetDefaultDllDirectoriesFunc)(DWORD DirectoryFlags);
typedef void* (WINAPI * AddDllDirectoryFunc)(PCWSTR NewDirectory);

#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#endif

bool RestartUpdate(LPCTSTR szPath)
{
	std::string restart_action;
	std::wstring action_path(szPath);
	action_path += RESTART_ACTION_PATH;

	base::CFile file;
	if (file.Open(base::kFileRead, action_path.c_str()))
	{
		base::CFileData fd;
		if (fd.Read(file))
		{
			fd.ToText(restart_action);
		}
		file.Close();
	}

	if (!restart_action.empty()) {
		Value* v = base::JSONReader::Read(restart_action.c_str(), true);
		if (v && v->GetType() == Value::TYPE_DICTIONARY)
		{
			DictionaryValue* dv = (DictionaryValue *)v;
			std::wstring run;
			ListValue* lv = NULL;

			if (dv->GetList("replace", &lv)) {
				ListValue::iterator iter = lv->begin();
				while (iter != lv->end())
				{
					if ((*iter)->GetType() == Value::TYPE_DICTIONARY)
					{
						std::wstring path;
						if (((DictionaryValue *)(*iter))->GetString("path", &path)) {
							std::wstring src_file(szPath), dst_file(szPath);

							src_file += L"temp/";
							src_file += path;
							dst_file += path;

							DeleteFile(dst_file.c_str());
							MoveFile(src_file.c_str(), dst_file.c_str());
						}
					}
					iter++;
				}
			}
			// run a file
			if (dv->GetString("run", &run)) {
				ShellExecute(NULL, _T("open"), run.c_str(), L"", L"", SW_SHOW);
			}
		}

		DeleteFile(action_path.c_str());

		return true;
	}

	return false;
}

bool CallAddDllDirectory(PCWSTR NewDirectory)
{
	HMODULE hKernel32 = LoadLibrary(L"kernel32.dll");
	if (hKernel32) {
		AddDllDirectoryFunc AddDllDirectory = (AddDllDirectoryFunc)GetProcAddress(hKernel32, "AddDllDirectory");
		SetDefaultDllDirectoriesFunc SetDefaultDllDirectories = (SetDefaultDllDirectoriesFunc)GetProcAddress(hKernel32, "SetDefaultDllDirectories");
		if (SetDefaultDllDirectories != NULL) {
			SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		}
		if (AddDllDirectory != NULL) {
			AddDllDirectory(NewDirectory);
			return true;
		}
	}
	return false;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

	TCHAR szPath[MAX_PATH];
	GetModuleFileNameW(NULL, szPath, MAX_PATH);
	for (int i = lstrlen(szPath) - 1; i > 0; i--)
	{
		if (szPath[i] == '\\')
		{
			szPath[i + 1] = 0;
			break;
		}
	}

	RestartUpdate(szPath);

	std::wstring current_version;
	std::wstring version_path(szPath);
	version_path += VERSION_PATH;

	base::CFile file;
	if (file.Open(base::kFileRead, version_path.c_str()))
	{
		base::CFileData fd;
		if (fd.Read(file))
		{
			fd.ToText(current_version);
		}
		file.Close();
	}

	if (!current_version.empty()) {
		std::wstring dll_path(szPath);
#ifdef _DEBUG
		current_version = _T("data");
#endif
		dll_path += current_version;
		CallAddDllDirectory(dll_path.c_str());

		dll_path += _T("\\missevan-fm.dll");

		HMODULE hDll = LoadLibrary(dll_path.c_str());
		if (hDll != NULL) {
			int ret = 1;
			MissEvanFMMainFunc MissEvanFMMain = (MissEvanFMMainFunc)GetProcAddress(hDll, "MissEvanFMMain");

			if (MissEvanFMMain != NULL) {
				ret = MissEvanFMMain(hInstance);
			}

			FreeLibrary(hDll);

			if (MissEvanFMMain != NULL) {
				return ret;
			}
		}
	}

	MessageBox(NULL, _T("程序加载失败！"), _T("错误"), MB_ICONERROR);
	return 1;
}
