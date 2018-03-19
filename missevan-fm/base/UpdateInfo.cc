
#include "stdafx.h"
#include "UpdateInfo.h"
#include <base/json/values.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/string/string_split.h>
#include <base/string/string_number_conversions.h>
#include <base/md5.h>
#include <base/file/file.h>
#include <base/file/filedata.h>
#include <lzmahelper.h>
#include <common/buffer.h>

#include <shlobj.h>
#include <shellapi.h>

#include <common/string_easy_conv.h>

#define VERSION_FILE L"VERSION"

#define VERSION_PATH (DATA_FOLDER L"\\" VERSION_FILE)
#define RESTART_ACTION_PATH L"temp\\restart.json"

#pragma pack(1) //指定按2字节对齐
typedef struct _TA_LZMA_HEADER {
	char			magic[5];	/*TALZMA*/
	unsigned char	props[5];
	size_t			originalsize;
	size_t			compresssize;
} TA_LZMA_HEADER;
#pragma pack() //取消指定对齐，恢复缺省对齐

CUpdateInfo::CUpdateInfo(LPCWSTR path)
{
	m_path = path;
	if (m_path[m_path.length() - 1] != L'\\')
	{
		m_path += L"\\";
	}
}

CUpdateInfo::~CUpdateInfo()
{
}

LPCSTR CUpdateInfo::GetUrlFileName(LPCSTR url)
{
	for (int i = lstrlenA(url) - 1; i > 0; i--)
	{
		if (url[i] == '/')
		{
			return url + i + 1;
			break;
		}
	}
	return url;
}

LPCWSTR CUpdateInfo::GetUrlFileName(LPCWSTR url)
{
	for (int i = lstrlen(url) - 1; i > 0; i--)
	{
		if (url[i] == '/')
		{
			return url + i + 1;
			break;
		}
	}
	return url;
}

LPCWSTR CUpdateInfo::GetFileNameExt(LPCWSTR file)
{
	for (int i = lstrlen(file) - 1; i > 0; i--)
	{
		if (file[i] == '.')
		{
			return file + i + 1;
			break;
		}
	}
	return file;
}

bool CUpdateInfo::Load(LPCSTR json_updateinfo)
{
	Value* v = base::JSONReader::Read(json_updateinfo, true);
	if (v)
	{
		if (v->GetType() == Value::TYPE_DICTIONARY)
		{
			DictionaryValue* dv = (DictionaryValue *)v;
			dv->GetString("version", &version);

			//比较版本
			//CompareVersion

			//更新完成后的运行的文件
			dv->GetString("run", &run);

			dv->GetString("openurl", &openurl);

			std::string utf8tips;
			dv->GetString("tips", &utf8tips);
			wchar_t* wtips = ::utf8ToWcString(utf8tips.c_str());
			m_tips = wtips;
			free(wtips);

			ListValue *lv = NULL;
			if (dv->Get("files", (Value **)&lv))
			{
				if (lv->GetType() == Value::TYPE_LIST)
				{
					ListValue::iterator iter = lv->begin();
					while (iter != lv->end())
					{
						if ((*iter)->GetType() == Value::TYPE_DICTIONARY)
						{
							UPDATE_FILE uf;
							uf.needreload = true;
							uf.file_size = 0;
							((DictionaryValue *)(*iter))->GetString("url", &uf.url);
							((DictionaryValue *)(*iter))->GetString("path", &uf.path);
							((DictionaryValue *)(*iter))->GetString("md5", &uf.md5);
							((DictionaryValue *)(*iter))->GetInteger("size", &uf.size);
							// decompress info
							((DictionaryValue *)(*iter))->GetString("file_md5", &uf.file_md5);
							((DictionaryValue *)(*iter))->GetInteger("file_size", &uf.file_size);

							m_vecfiles.push_back(uf);
						}
						iter++;
					}

					return GetFilesCount() > 0;
				}
			}
		}
	}
	return false;
}

bool CUpdateInfo::HasRun()
{
	return run.length() > 0;
}

void CUpdateInfo::Run()
{
	if (HasRun())
	{
		std::wstring filepath = m_path;
		filepath += run;
		::ShellExecute(NULL, _T("open"), filepath.c_str(), L"", L"", SW_SHOW);
	}
}

void CUpdateInfo::OpenURL()
{
	if (openurl.length() > 0)
	{
		::ShellExecute(NULL, _T("open"), openurl.c_str(), L"", L"", SW_SHOW);
	}
}

bool CUpdateInfo::CompareVersion()	//ture为版本较旧
{
	std::wstring filename = m_path;
	filename += VERSION_PATH;

	base::CFile file;
	if (file.Open(base::kFileRead, filename.c_str()))
	{
		base::CFileData fd;
		if (fd.Read(file))
		{
			std::string current_version;
			fd.ToText(current_version);

			return CompareVersion(current_version);
		}
	}
	return true;
}

bool CUpdateInfo::CompareVersion(std::string& current_version)
{
	std::vector<std::string> ver_server, ver_current;
	base::SplitString(version, L'.', &ver_server);
	base::SplitString(current_version, L'.', &ver_current);

	int major_server = 0, major_current = 0;
	size_t version_index = 0;
#ifdef _DEBUG
	return true;
#endif
	while (true)
	{
		base::HexStringToInt(ver_server[version_index].c_str(), NULL, &major_server);
		base::HexStringToInt(ver_current[version_index].c_str(), NULL, &major_current);

		if (major_server < major_current)
		{
			//本地的更新！
			return false;
		} else if (major_server > major_current) {
			return true;
		} else {
			version_index++;
			if (!(ver_server.size() > version_index && ver_current.size() > version_index))
			{
				//相同
				return false;
			}
		}
	}
}

bool CUpdateInfo::NeedToUpdate()
{
	return CompareVersion();
}

bool CUpdateInfo::SaveVersion()
{
	std::wstring filename = m_path;
	filename += DATA_FOLDER;
	::CreateDirectory(filename.c_str(), NULL);
	filename += (L"\\" VERSION_FILE);

	if (m_replacelist.size() > 0) {
		// save to `temp/restart.json`
		DictionaryValue v;
		ListValue *lv = new ListValue();
		for (size_t i = 0; i < m_replacelist.size(); i++) {
			DictionaryValue *dv = new DictionaryValue();
			dv->SetString("path", m_replacelist[i].path);
			lv->Append(dv);
		}
		v.Set("replace", lv);

		std::string str_json;
		base::JSONWriter::Write(&v, false, &str_json);

		std::wstring path(m_path);
		path += RESTART_ACTION_PATH;

		base::CFile f;
		if (f.Open(base::kFileCreate, path.c_str())) {
			f.Write((uint8 *)str_json.c_str(), str_json.length());
			f.Close();
		}
	}

	//delete old version file
	::DeleteFile(filename.c_str());

	CBuffer buf;
	buf.Write((unsigned char *)version.c_str(), version.length());
	return buf.FileWrite(filename.c_str());
}

bool CUpdateInfo::NeedToDownload(int iIndex)
{
	UPDATE_FILE& uf = m_vecfiles[iIndex];
	base::CFile file;
	std::wstring filename = m_path;
	std::wstring tmppath = uf.path;
	for (size_t i = 0; i < tmppath.length(); i++)
	{
		if (tmppath[i] == L'/') tmppath[i] = L'\\';
	}
	filename += tmppath;

	//不为talzma才需要检查
	if (file.Open(base::kFileRead, filename.c_str()))
	{
		if (lstrcmpi(GetFileNameExt(uf.url.c_str()), L"talzma") != 0)
		{
			// 非压缩过的文件
			if ((int)file.GetFileSize() == uf.size)
			{
				base::CFileData fd;
				if (fd.Read(file))
				{
					base::MD5Digest md5digest = {0};
					base::MD5Sum(fd.GetData(), fd.GetSize(), &md5digest);
					if (base::MD5DigestToBase16(md5digest) == uf.md5)
					{
						uf.needreload = false;
						return false;
					}
				}
			}
		} else if (uf.file_size > 0 && !uf.file_md5.empty()) {
			if ((int)file.GetFileSize() == uf.file_size)
			{
				base::CFileData fd;
				if (fd.Read(file))
				{
					base::MD5Digest md5digest = {0};
					base::MD5Sum(fd.GetData(), fd.GetSize(), &md5digest);
					if (base::MD5DigestToBase16(md5digest) == uf.file_md5)
					{
						uf.needreload = false;
						return false;
					}
				}
			}
		} else {
			base::CFileData fd;
			if (fd.Read(file))
			{
				CBuffer talzmabuf;
				// 需要压缩数据检查
				BYTE props[LZMA_PROPS_SIZE] = {0};
				size_t propsize = LZMA_PROPS_SIZE;
				size_t osize = fd.GetSize();
				size_t csize = osize * 3;
				BYTE *bcom = (BYTE *)malloc(csize);
				if (bcom)
				{
					/* *outPropsSize must be = 5 */
					int status = LzmaCompress(bcom, &csize, fd.GetData(), fd.GetSize(), props, &propsize,
							5,		/* 0 <= level <= 9, default = 5 */
							65536,	/* default = (1 << 24) */
							3,		/* 0 <= lc <= 8, default = 3  */
							0,		/* 0 <= lp <= 4, default = 0  */
							2,		/* 0 <= pb <= 4, default = 2  */
							32,		/* 5 <= fb <= 273, default = 32 */
							1);		/* 1 or 2, default = 2 */
					if (status == SZ_OK)
					{
						if (sizeof(TA_LZMA_HEADER) + csize == uf.size)
						{
							TA_LZMA_HEADER header;
							memcpy(header.magic, "TALZMA", 5);
							memcpy(header.props, props, LZMA_PROPS_SIZE);
							header.originalsize = fd.GetSize();
							header.compresssize = csize;

							talzmabuf.Write((unsigned char *)&header, sizeof(TA_LZMA_HEADER));
							talzmabuf.Write(bcom, csize);

							base::MD5Digest md5digest = {0};
							base::MD5Sum(talzmabuf.GetBuffer(), talzmabuf.GetBufferLen(), &md5digest);
						
							if (::lstrcmpiA(base::MD5DigestToBase16(md5digest).c_str(), uf.md5.c_str()) == 0)
							{
								free(bcom);
								uf.needreload = false;
								return false;
							}
						}
					}
					free(bcom);
				}
			}
		}
	}
	return true;
}

bool CUpdateInfo::CheckFile(int iIndex, LPCWSTR path)
{
	base::CFile file;
	if (file.Open(base::kFileRead, path))
	{
		if ((int)file.GetFileSize() == m_vecfiles[iIndex].size)
		{
			base::CFileData fd;
			if (fd.Read(file))
			{
				base::MD5Digest md5digest = {0};
				base::MD5Sum(fd.GetData(), fd.GetSize(), &md5digest);
				if (::lstrcmpiA(base::MD5DigestToBase16(md5digest).c_str(), m_vecfiles[iIndex].md5.c_str()) == 0)
				{
					return true;
				}
			}
		}
	}
	return false;
}

CUpdateInfo::UpdateStat CUpdateInfo::UpdateFile(int iIndex)
{
	if (!m_vecfiles[iIndex].needreload) {
		return kUSLatest;
	}

	std::vector<std::wstring> r;
	std::wstring path = m_path, tmppath = m_path;
	base::SplitString(m_vecfiles[iIndex].path, L'/', &r);
	if (r.size() > 1)
	{
		for (size_t i = 0; i < r.size() - 1; i++)
		{
			path += r[i];
			::CreateDirectory(path.c_str(), NULL);
			path += L'\\';
		}
		path += r[r.size() - 1];
	} else {
		path += m_vecfiles[iIndex].path;
	}

	tmppath += (TEMP_FOLDER L"\\");
	tmppath += GetUrlFileName(m_vecfiles[iIndex].url.c_str());

	if (lstrcmpi(GetFileNameExt(m_vecfiles[iIndex].url.c_str()), L"talzma") == 0)
	{
		bool uncompress_error = true;
		bool update_on_reload = false;
		//压缩数据
		base::CFile file;
		if (file.Open(base::kFileRead, tmppath.c_str()))
		{
			base::CFileData fd;
			if (fd.Read(file))
			{
				//至少要有头的大小
				if (fd.GetSize() > sizeof(TA_LZMA_HEADER))
				{
					DeleteFile(path.c_str());
					CBuffer dstfile_buf;

					//解压数据
					TA_LZMA_HEADER* header = (TA_LZMA_HEADER *)fd.GetData();
					if (memcmp(header->magic, "TALZMA", 5) == 0)
					{
						if (header->compresssize + sizeof(TA_LZMA_HEADER) <= fd.GetSize())
						{
							size_t osize = header->originalsize;
							size_t csize = header->compresssize;
							size_t propsize = LZMA_PROPS_SIZE;
							unsigned char* m_original_data = (unsigned char *)malloc(osize);
							if (m_original_data)
							{
								int status = LzmaUncompress(m_original_data, &osize, fd.GetData() + sizeof(TA_LZMA_HEADER), (size_t *)&csize, header->props, propsize);
								if (status == SZ_OK)
								{
									if (dstfile_buf.Write(m_original_data, osize))
									{
										uncompress_error = false;
									}
								}
							}
						}
					}

					if (!uncompress_error) {
						if (!dstfile_buf.FileWrite(path.c_str())) {
							// tmppath remove last .talzma
							if (!dstfile_buf.FileWrite(tmppath.substr(0, tmppath.length() - 7).c_str())) {
								return kUSError;
							}
							update_on_reload = true;
						}
					}
				}
			}
			file.Close();
		}
		DeleteFile(tmppath.c_str());

		if (uncompress_error)
		{
			return kUSError;
		}
		if (update_on_reload) {
			REPLACE_FILE rf;
			rf.path = m_vecfiles[iIndex].path;
			m_replacelist.push_back(rf);
			return kUSUpdateOnReload;
		}
	} else {
		DeleteFile(path.c_str());
		if (!::MoveFile(tmppath.c_str(), path.c_str()))
		{
			REPLACE_FILE rf;
			rf.path = m_vecfiles[iIndex].path;
			m_replacelist.push_back(rf);
			return kUSUpdateOnReload;
		}
	}
	return kUSUpdated;
}

int CUpdateInfo::GetTotalSize()
{
	int totalsize = 0;
	for (size_t i = 0; i < m_vecfiles.size(); i++)
	{
		totalsize += m_vecfiles[i].size;
	}
	return totalsize;
}

int CUpdateInfo::GetFilesCount()
{
	return m_vecfiles.size();
}

UPDATE_FILE& CUpdateInfo::GetFile(int iIndex)
{
	return m_vecfiles[iIndex];
}