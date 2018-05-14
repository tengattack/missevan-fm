
#ifndef _SNOW_UPDATE_H_
#define _SNOW_UPDATE_H_ 1
#pragma once

#include <string>
#include <vector>
#include <base/file/file.h>

#ifdef _SNOW_ZERO
#define DATA_FOLDER	L"Save"
#define TEMP_FOLDER	L"Temp"
#else
#define DATA_FOLDER	L"data"
#define TEMP_FOLDER	L"temp"
#endif


typedef struct _UPDATE_FILE {
	std::wstring url;
	std::wstring path;
	std::string md5;
	std::string file_md5;
	int size;
	int file_size;
	bool needreload;
} UPDATE_FILE;

typedef struct _REPLACE_FILE {
	std::wstring path;
} REPLACE_FILE;


//声明一个文件结构体 
struct DownloadFile 
{ 
	wchar_t		*filename;
	bool		*stopped;
	base::CFile	*stream;
	int			currentpos;
	int			totalsize;
};

class CUpdateInfo {
public:

	enum UpdateStat {
		kUSError = -1,
		kUSUpdated = 0,
		kUSLatest,
		kUSUpdateOnReload,
	};

	CUpdateInfo(LPCWSTR path);
	~CUpdateInfo();

	static LPCSTR GetUrlFileName(LPCSTR url);
	static LPCWSTR GetUrlFileName(LPCWSTR url);
	static LPCWSTR GetFileNameExt(LPCWSTR file);

	UpdateStat UpdateFile(int iIndex);

	bool Load(LPCSTR json_updateinfo);
	bool NeedToDownload(int iIndex);
	bool NeedToUpdate();
	bool SaveVersion();

	bool HasRun();
	void Run();
	void OpenURL();
	
	int GetFilesCount();
	int GetTotalSize();

	bool CheckFile(int iIndex, LPCWSTR path);

	UPDATE_FILE& GetFile(int iIndex);

	inline bool HasTips()
	{
		return (m_tips.length() > 0);
	}
	inline LPCWSTR GetTips()
	{
		return m_tips.c_str();
	}
	inline std::string& GetVersion()
	{
		return version;
	}

protected:
	std::vector<UPDATE_FILE> m_vecfiles;
	std::vector<REPLACE_FILE> m_replacelist;
	std::wstring run;
	std::wstring m_tips;
	std::wstring openurl;
	std::string version;

	std::wstring m_path;

	bool CompareVersion();
	bool CompareVersion(std::string& current_version);
};

#endif