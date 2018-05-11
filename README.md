# missevan-fm

## Dependencies

### [vcpkg](https://github.com/Microsoft/vcpkg) 管理

* boost
* curl
* jsoncpp
* openssl
* websocketpp
* zlib
```sh
vcpkg install boost curl jsoncpp openssl websocketpp zlib
```

### 其他

* lzma
* [fr](https://github.com/tengattack/fr)
* [Agora_Native_SDK_for_Windows](https://agora.io/)
* [NIM_PC_SDK](http://netease.im/im-sdk-demo)
* [LivePlayer_Windows_SDK](http://netease.im/im-sdk-demo)

## Directory Structure

```txt
├── missevan-fm/
│   └── ...
├── sdks/
│   ├── Agora_Native_SDK_for_Windows_v2.1.3_FULL/
│   ├── fr/
│   ├── lib/
│   │   ├── debug-vc100/
│   │   ├── static-vc100/
│   │   └── ...
│   ├── LivePlayer_Windows_SDK_v1.0.1/
│   ├── lzma920/
│   ├── NIM_PC_SDK_x86_x64_v4.9.0/
│   └── lzmahelper.h
```
