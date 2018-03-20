
#ifndef _MFM_DEIVCE_MANAGER_H_
#define _MFM_DEIVCE_MANAGER_H_

#pragma once

#include <string>
#include <list>
#include <base/basictypes.h>

namespace base {
  template <typename T>
  struct DefaultSingletonTraits;
}

class DeviceManager
{
public:

	struct MEDIA_DEVICE_DRIVE_INFO
	{
		std::string device_path_;
		std::string friendly_name_;
	};

	enum DeviceSessionType
	{
		kDeviceSessionTypeNone = 0x0,
		kDeviceSessionTypeChat = 0x1,
		kDeviceSessionTypeSetting = 0x2,
		kDeviceSessionTypeRts = 0x4,
		kDeviceSessionTypeChatRoom = 0x8,
	};

	static DeviceManager *GetInstance();

	void GetDeviceByJson(bool ret, nim::NIMDeviceType type, const char* json);
	std::list<MEDIA_DEVICE_DRIVE_INFO> GetDeviceInfo(nim::NIMDeviceType type);
	bool GetDefaultDevicePath(int &no, std::string &device_path, nim::NIMDeviceType type);
	void SetDefaultDevicePath(std::string device_path, nim::NIMDeviceType type);

	void StartDevice(nim::NIMDeviceType type, std::string device_path, DeviceSessionType session_type);
	void EndDevice(nim::NIMDeviceType type, DeviceSessionType session_type, bool forced = false);

	void StartAudioDevice();
	void EndAudioDevice();

	bool IsAudioHooked();
	void StartHookAudio(const std::string& path);
	void EndHookAudio();

	bool IsMicOpened();
	void OpenMic();
	void CloseMic();

	/** SetVolume
	* vol: 0~1
	* capture true 标识设置麦克风音量，false 标识设置播放音量
	*/
	void SetVolume(float vol, bool capture);

protected:

	DeviceManager();
	~DeviceManager();

	std::list<MEDIA_DEVICE_DRIVE_INFO> device_info_list_[4];
	std::string def_device_path_[4];
	DWORD device_session_type_[4];
	bool m_hook_audio;
	bool m_open_mic;

	static DeviceManager *deviceManager;
	static void EnumDevCb(bool ret, nim::NIMDeviceType type, const char* json, const void *user_data);
	static void StartDeviceCb(nim::NIMDeviceType type, bool ret, const char *json_extension, const void *user_data);
	static void DeviceStatusCb(nim::NIMDeviceType type, UINT status, const char* path, const char *json, const void *user_data);

	void OnDeviceStatus(nim::NIMDeviceType type, UINT status, std::string path);

private:
	friend struct base::DefaultSingletonTraits<DeviceManager>;
	DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

#endif
