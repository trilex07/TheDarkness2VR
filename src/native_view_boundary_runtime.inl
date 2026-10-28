static void InstallNativeViewBoundary() {
    if(!g_nativeViewBoundaryMode || g_nativeViewBoundary.Installed())return;
    if(!g_imagePreservingPresent || !g_nativeImageGuard.enabled || g_imagePresent.Failed() ||
       g_submitMode!=4 || g_stereoMode!=1 || !g_autoFovWiden || g_engineFovFactor>1 ||
       GetPrivateProfileIntA("Diagnostics","NativeCameraCover",0,".\\Darkness2VR-diagnostic.ini")) {
        Log("[NativeViewBoundary] refused configuration; baseline retained");return;
    }
    const auto image=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    char path[MAX_PATH]{};if(!GetModuleFileNameA(nullptr,path,MAX_PATH))return;
    const auto sha=matched_capture::HashFile(path);
    if(sha!="d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1" ||
       reinterpret_cast<uintptr_t>(image)!=0x400000) {
        Log("[NativeViewBoundary] refused EXE identity");return;
    }
    const auto read=[](uint32_t address,void* output,size_t bytes)->bool {
        __try {memcpy(output,reinterpret_cast<const void*>(address),bytes);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    };
    uint32_t failed=0;
    if(!d2vr_camera_cover::Validate(read,failed)) {
        Log("[NativeViewBoundary] refused native camera contract at %08X",failed);return;
    }
    const d2vr_camera_cover::Pin pins[]={
        {0xA66330,"\x83\xec\x44\x53\x55\x56\x8b\xf1\x8b\x46\x08\x33\xdb",13},
        {0xA66465,"\x8b\x2d\x5c\xb3\xee\x00",6},
        {0xA6647C,"\x89\x5c\x24\x2c\x89\x5c\x24\x30\xff\xd5\x8b\x54\x24\x28",14},
        {0xA6676A,"\xdd\xd8\x8d\x54\x24\x28\x52\x89\x5c\x24\x2c\x89\x5c\x24\x30\xff\xd5\x5e\x5d\x5b\x83\xc4\x44\xc3",24},
        {0xFC89E0,"\x30\x63\xa6\x00",4},
        {0xFC68B0,"\x30\x63\xa6\x00",4},
        {0xFC89D8,"\xb0\xf1\x99\x00",4},
        {0x99F1B0,"\xe9\x6b\x95\xee\xff",5},
        {0xFC68A8,"\x20\x87\x88\x00",4},
        {0x888720,"\x83\xec\x1c\x53\x55\x56\x8b\xf1\x8b\x06\x8b\x50\x24\x57",14},
        {0x888965,"\x8b\x8e\xa8\x00\x00\x00\x8b\x09\xe8\x0e\xd4\x0f\x00",13}
    };
    unsigned char data[40];
    for(const auto& pin:pins)if(!read(pin.address,data,pin.size) || memcmp(data,pin.bytes,pin.size)) {
        Log("[NativeViewBoundary] refused native view contract at %08X",pin.address);return;
    }
    const auto kernel=GetModuleHandleW(L"kernel32.dll");
    const auto target=reinterpret_cast<void*>(GetProcAddress(kernel,"QueryPerformanceCounter"));
    if(!target || *reinterpret_cast<void*const*>(image+0xAEB35C)!=target) {
        Log("[NativeViewBoundary] refused original QPC import identity");return;
    }
    // The final +B0 QPC is too late in real gameplay (had3D=1). Select its
    // first QPC, before virtual+114 and the remaining update dispatches.
    // ESI=this at both sites. +A8 dispatches through 99F1B0 to 888720, whose 88896D call
    // to 985D80 is present in the retained real-game ETW stacks. +F0's
    // 6DBE80 is a separate test-render path, not the captured gameplay path.
    const bool installed=g_nativeViewBoundary.Start(target,0xA66486,ObserveNativeViewBoundary,g_nativeImageGuard);
    Log("[NativeViewBoundary] installed=%d mode=%d external QPC caller=00A66486; ESI view +A8 -> renderer +3F4 -> camera; mode2=culling-only after corroboration; EXE/IAT unchanged",
        installed,g_nativeViewBoundaryMode);
}
