#pragma once

#ifdef _MFM_DLL
#define MFM_EXPORT    __declspec(dllexport)
#else
#define MFM_EXPORT    __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MFM_DLL
MFM_EXPORT int WINAPI MissEvanFMMain(HINSTANCE hInstance);
#endif

#ifdef __cplusplus
}
#endif