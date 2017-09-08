
#include "stdafx.h"

#include <base/file/file.h>
#include <base/file/filedata.h>
#include <base/json/values.h>
#include <base/json/json_reader.h>

#include "global.h"
#include "config.h"

namespace config {
	std::string proxy_url;

	bool getConfigData(const std::string& data)
	{
		bool ret = false;
		Value *v = base::JSONReader::Read(data, true);
		if (v) {
			if (v->GetType() == Value::TYPE_DICTIONARY) {
				DictionaryValue *dv = (DictionaryValue *)v;
				dv->GetString("proxy", &proxy_url);
				ret = true;
			}
			delete v;
		}
		return ret;
	}

	bool read()
	{
		std::wstring config_path(global::wpath);
		config_path += L"config.json";

		base::CFile file;
		if (file.Open(base::kFileRead, config_path.c_str())) {
			base::CFileData fd;
			if (fd.Read(file)) {
				std::string data;
				fd.ToText(data);
				return getConfigData(data);
			}
		}

		return false;
	}
};