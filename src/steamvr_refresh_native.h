#pragma once
#include "steamvr_refresh.h"
#include "../third_party/openvr/openvr.h"
#include <windows.h>
#include <string>
#pragma comment(lib,"advapi32.lib")

namespace d2vr_refresh {
struct SteamNative {
    HMODULE module=nullptr;
    vr::IVRSystem* system=nullptr;
    decltype(&vr::VR_InitInternal2) init=nullptr;
    decltype(&vr::VR_ShutdownInternal) shutdown=nullptr;
    decltype(&vr::VR_GetGenericInterface) interfaceFn=nullptr;
    bool Load() {
        // Refuse an already loaded client: its owner may have initialized it.
        if(GetModuleHandleW(L"openvr_api.dll"))return false;
        wchar_t root[1024]{};DWORD bytes=sizeof(root);
        if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",
            RRF_RT_REG_SZ,nullptr,root,&bytes)!=ERROR_SUCCESS || !root[0] || root[1]!=L':' ||
            (root[2]!=L'\\' && root[2]!=L'/'))return false;
        // Optional conventional installation only. Custom libraries retain
        // UNKNOWN rather than searching disk or loading a game-local wrapper.
        const std::wstring path=std::wstring(root)+L"\\steamapps\\common\\SteamVR\\bin\\win32\\openvr_api.dll";
        wchar_t full[2048]{};
        const DWORD length=GetFullPathNameW(path.c_str(),2048,full,nullptr);
        if(!length || length>=2048 ||
           !(full[0] && full[1]==L':'))return false;
        module=LoadLibraryExW(full,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module)return false;
        init=reinterpret_cast<decltype(init)>(GetProcAddress(module,"VR_InitInternal2"));
        shutdown=reinterpret_cast<decltype(shutdown)>(GetProcAddress(module,"VR_ShutdownInternal"));
        interfaceFn=reinterpret_cast<decltype(interfaceFn)>(GetProcAddress(module,"VR_GetGenericInterface"));
        if(!init || !shutdown || !interfaceFn){Unload();return false;}
        return true;
    }
    bool Initialize() {
        vr::EVRInitError error=vr::VRInitError_Unknown;
        init(&error,vr::VRApplication_Background,nullptr);
        return error==vr::VRInitError_None;
    }
    bool System() {
        vr::EVRInitError error=vr::VRInitError_Unknown;
        system=static_cast<vr::IVRSystem*>(interfaceFn(vr::IVRSystem_Version,&error));
        return error==vr::VRInitError_None && system;
    }
    float Frequency() {
        if(!system || !system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd) ||
           system->GetTrackedDeviceClass(vr::k_unTrackedDeviceIndex_Hmd)!=vr::TrackedDeviceClass_HMD)return 0;
        vr::ETrackedPropertyError error=vr::TrackedProp_Success;
        const float value=system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
            vr::Prop_DisplayFrequency_Float,&error);
        return error==vr::TrackedProp_Success?value:0;
    }
    void Shutdown(){system=nullptr;shutdown();}
    void Unload(){system=nullptr;if(module)FreeLibrary(module);module=nullptr;init=nullptr;shutdown=nullptr;interfaceFn=nullptr;}
};
}
