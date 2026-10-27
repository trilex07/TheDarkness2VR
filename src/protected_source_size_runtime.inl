static void InstallProtectedSourceQuality() {
    if(g_sourceQualityMode==d2vr_source_quality::Mode::Off) return;
    if(!g_imagePreservingPresent || !g_nativeImageGuard.enabled || g_imagePresent.Failed()) {
        Log("[SourceQuality] refused: protected external Present architecture required");return;
    }
    const auto image=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    char path[MAX_PATH]{};
    if(!GetModuleFileNameA(nullptr,path,MAX_PATH)) return;
    const auto sha=matched_capture::HashFile(path);
    const unsigned char call[]={0xff,0x15,0x78,0xb4,0xee,0x00};
    const auto user32=GetModuleHandleW(L"user32.dll");
    const auto target=reinterpret_cast<void*>(GetProcAddress(user32,"GetClientRect"));
    if(!d2vr_present_patch::Identity(sha.c_str(),pe->FileHeader.Machine,pe->FileHeader.TimeDateStamp,
            pe->OptionalHeader.SizeOfImage,pe->OptionalHeader.ImageBase,pe->OptionalHeader.Magic) ||
       reinterpret_cast<uintptr_t>(image)!=0x400000 ||
       memcmp(image+0x89646e,call,sizeof(call)) || !target ||
       *reinterpret_cast<void*const*>(image+0xaeb478)!=target) {
        Log("[SourceQuality] refused: original SHA/PE/native sizing call/import mismatch; baseline retained");return;
    }
    const bool ready=g_protectedSourceSize.Start(target,g_nativeImageGuard,g_sourceQualityMode,
        g_renderSizeHwnd,reinterpret_cast<uintptr_t>(image)+0x896474,Log,
        {unsigned(g_renderW),unsigned(g_renderH)});
    Log("[SourceQuality] installed=%d mode=%u; engine owns dependent RT/depth/viewport creation; no resource substitution",ready,unsigned(g_sourceQualityMode));
}
