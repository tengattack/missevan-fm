#include "stdafx.h"
#include "DeviceManager.h"

#include <base/logging.h>
#include <base/memory/singleton.h>
#include "audio/AudioTransform.h"

DeviceManager *DeviceManager::deviceManager = NULL;

DeviceManager *DeviceManager::GetInstance()
{
	return base::Singleton<DeviceManager>::get();
}

DeviceManager::DeviceManager()
	: m_hook_audio(false)
	, m_open_mic(false)
{
	for (int i = nim::kNIMDeviceTypeAudioIn; i <= nim::kNIMDeviceTypeVideo; i++)
	{
		device_session_type_[i] = kDeviceSessionTypeNone;
	}
	CAudioTransform::Init();
}

DeviceManager::~DeviceManager()
{
	CAudioTransform::Cleanup();
}

void DeviceManager::GetDeviceByJson(bool ret, nim::NIMDeviceType type, const char* json)
{
	device_info_list_[type].clear();
	if (ret)
	{
		Json::Value valus;
		Json::Reader reader;
		if (reader.parse(json, valus) && valus.isArray())
		{
			int num = valus.size();
			for (int i = 0; i<num; ++i)
			{
				Json::Value device;
				device = valus.get(i, device);
				MEDIA_DEVICE_DRIVE_INFO info;
				info.device_path_ = device[nim::kNIMDevicePath].asString();
				info.friendly_name_ = device[nim::kNIMDeviceName].asString();
				device_info_list_[type].push_back(info);
			}
		}
	}
}

std::list<DeviceManager::MEDIA_DEVICE_DRIVE_INFO> DeviceManager::GetDeviceInfo(nim::NIMDeviceType type)
{
	nim::VChat::EnumDeviceDevpath(type, &EnumDevCb);
	return device_info_list_[type];
}

// callbacks
void DeviceManager::EnumDevCb(bool ret, nim::NIMDeviceType type, const char* json, const void *user_data)
{
	DeviceManager::GetInstance()->GetDeviceByJson(ret, type, json);
}

void DeviceManager::StartDeviceCb(nim::NIMDeviceType type, bool ret, const char *json_extension, const void *user_data)
{
	switch (type)
	{
	case nim::kNIMDeviceTypeVideo:
		LOG(INFO) << "OnStartDeviceCb nim::kNIMDeviceTypeVideo " << ret;
		break;
	case nim::kNIMDeviceTypeAudioIn:
		LOG(INFO) << "OnStartDeviceCb nim::kNIMDeviceTypeAudioIn " << ret;
		if (!ret) {
			// TODO: show error
			DeviceManager::GetInstance()->CloseMic();
		}
		break;
	case nim::kNIMDeviceTypeAudioHook:
		LOG(INFO) << "OnStartDeviceCb nim::kNIMDeviceTypeAudioHook " << ret;
		if (!ret) {
			// TODO: show error
			DeviceManager::GetInstance()->m_hook_audio = false;
		}
		break;
	}
}

void DeviceManager::DeviceStatusCb(nim::NIMDeviceType type, UINT status, const char* path, const char *json, const void *user_data)
{
	DeviceManager::GetInstance()->OnDeviceStatus(type, status, path);
}

void DeviceManager::OnDeviceStatus(nim::NIMDeviceType type, UINT status, std::string path)
{
	if (device_session_type_[type] == kDeviceSessionTypeNone)
	{
		nim::VChat::RemoveDeviceStatusCb(type);
	} else if ((status & nim::kNIMDeviceStatusWorkRemove) || (status & nim::kNIMDeviceStatusEnd)) {
		if (type == nim::kNIMDeviceTypeAudioIn) {
			CloseMic();
		}
	}
}

bool DeviceManager::GetDefaultDevicePath(int &no, std::string &device_path, nim::NIMDeviceType type)
{
	if (type == nim::kNIMDeviceTypeAudioOutChat)
	{
		type = nim::kNIMDeviceTypeAudioOut;
	}
	no = 0;
	device_path.clear();
	GetDeviceInfo(type);
	if (device_info_list_[type].size() != 0)
	{
		if (!def_device_path_[type].empty())
		{
			int k = 0;
			for (auto i = device_info_list_[type].begin(); i != device_info_list_[type].end(); i++, k++)
			{
				if (i->device_path_ == def_device_path_[type])
				{
					no = k;
					device_path = def_device_path_[type];
					break;
				}
			}
		}
		if (device_path.empty())
		{
			no = 0;
			device_path = device_info_list_[type].begin()->device_path_;
			def_device_path_[type] = device_path;
		}
	}

	return !device_path.empty();
}

void DeviceManager::SetDefaultDevicePath(std::string device_path, nim::NIMDeviceType type)
{
	if (type == nim::kNIMDeviceTypeAudioOutChat)
	{
		type = nim::kNIMDeviceTypeAudioOut;
	}
	def_device_path_[type] = device_path;
}

void DeviceManager::StartDevice(nim::NIMDeviceType type, std::string device_path, DeviceSessionType session_type)
{
	if (device_path.empty())
	{
		int num_no = 0;
		GetDefaultDevicePath(num_no, device_path, type);
	}
	SetDefaultDevicePath(device_path, type);
	int width = 1280;
	int height = 720;
	nim::VChat::StartDevice(type, device_path, 50, width, height, &StartDeviceCb);
	if (device_session_type_[type] == kDeviceSessionTypeNone)
	{
		nim::VChat::AddDeviceStatusCb(type, &DeviceStatusCb);
	}
	device_session_type_[type] |= session_type;
}

void DeviceManager::EndDevice(nim::NIMDeviceType type, DeviceSessionType session_type, bool forced)
{
	device_session_type_[type] &= ~session_type;
	if (device_session_type_[type] == kDeviceSessionTypeNone || forced)
	{
		nim::VChat::EndDevice(type);
	}
}

void DeviceManager::StartAudioDevice()
{
	std::string def_device;
	int no = 0;

	//nim_comp::VideoManager::GetInstance()->
	GetDefaultDevicePath(no, def_device, nim::kNIMDeviceTypeAudioOutChat);
	//nim_comp::VideoManager::GetInstance()->
	StartDevice(nim::kNIMDeviceTypeAudioOutChat, def_device, kDeviceSessionTypeChatRoom);

	OpenMic();
}

void DeviceManager::EndAudioDevice()
{
	if (m_hook_audio) {
		nim::VChat::EndDevice(nim::kNIMDeviceTypeAudioHook);
		m_hook_audio = false;
	}
	CloseMic();
	//VideoManager::GetInstance()->
	EndDevice(nim::kNIMDeviceTypeAudioOutChat, kDeviceSessionTypeChatRoom);
}

bool DeviceManager::IsAudioHooked()
{
	return m_hook_audio;
}

void DeviceManager::StartHookAudio(const std::string& path)
{
	nim::VChat::EndDevice(nim::kNIMDeviceTypeAudioHook);
	nim::VChat::StartDevice(nim::kNIMDeviceTypeAudioHook, path, 0, 0, 0, &StartDeviceCb);
	m_hook_audio = true;
}

void DeviceManager::EndHookAudio()
{
	nim::VChat::EndDevice(nim::kNIMDeviceTypeAudioHook);
	m_hook_audio = false;
}

bool DeviceManager::IsMicOpened()
{
	return m_open_mic;
}

void DeviceManager::OpenMic()
{
	std::string def_device;
	int no = 0;
	GetDefaultDevicePath(no, def_device, nim::kNIMDeviceTypeAudioIn);
	StartDevice(nim::kNIMDeviceTypeAudioIn, def_device, kDeviceSessionTypeChatRoom);
	m_open_mic = true;

	nim::VChat::SetAudioProcess(false, true, false);
}

void DeviceManager::CloseMic()
{
	EndDevice(nim::kNIMDeviceTypeAudioIn, kDeviceSessionTypeChatRoom);
	m_open_mic = false;

	nim::VChat::SetAudioProcess(false, false, false);
}

void DeviceManager::SetVolume(float vol, bool capture)
{
	if (vol > 1) {
		vol = 1;
	} else if (vol < 0) {
		vol = 0;
	}
	unsigned char volumn = (unsigned char)(vol * 255);
	nim::VChat::SetAudioVolumn(volumn, capture);
}