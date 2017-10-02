
#include "stdafx.h"

#include <base/file/file.h>
#include <base/file/filedata.h>
#include <base/json/values.h>
#include <base/json/json_reader.h>

#include "global.h"
#include "config.h"

namespace config {
	std::string proxy_url;
	int audio_bitrate = 192;

	bool getConfigData(const std::string& data)
	{
		bool ret = false;
		Value *v = base::JSONReader::Read(data, true);
		if (v) {
			if (v->GetType() == Value::TYPE_DICTIONARY) {
				DictionaryValue *t;
				DictionaryValue *dv = (DictionaryValue *)v;
				dv->GetString("proxy", &proxy_url);
				if (dv->GetDictionary("audio", &t)) {
					if (t->GetInteger("bitrate", &audio_bitrate)) {
						if (audio_bitrate < 64 || audio_bitrate > 1200) {
							// reset to default
							audio_bitrate = 192;
						}
					}
				}
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