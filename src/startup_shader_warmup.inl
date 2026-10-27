// Runs once on the first successful device-creation thread, before returning
// the device to the engine. Never called from Present, Reset or DllMain.
static void StartupShaderWarmup(IDirect3DDevice9* dev) try {
    if(!g_startupShaderWarmup || !dev) return;
    static volatile LONG attempted=0;
    if(InterlockedCompareExchange(&attempted,1,0)!=0) return;
    wchar_t base[MAX_PATH]{};
    if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,base))) return;
    const std::wstring path=std::wstring(base)+L"\\Darkness2VR\\startup-shaders.bin";
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE) {Log("[StartupShaderWarmup] package unavailable error=%lu",GetLastError());return;}
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size) || size.QuadPart<8 || size.QuadPart>d2vr_startup_shader::MaxBytes) {
        CloseHandle(file);Log("[StartupShaderWarmup] rejected package size");return;
    }
    // DWORD storage ensures alignment for CreateShader and is freed before return.
    std::vector<DWORD> storage;
    try {storage.resize((size_t(size.QuadPart)+3)/4);} catch(...) {CloseHandle(file);return;}
    DWORD read=0;
    const BOOL ok=ReadFile(file,storage.data(),DWORD(size.QuadPart),&read,nullptr);
    CloseHandle(file);
    if(!ok || read!=DWORD(size.QuadPart)) {Log("[StartupShaderWarmup] incomplete read");return;}
    struct Ops {
        IDirect3DDevice9* device;
        uint64_t Now() {return GetTickCount64();}
        bool CreateAndRelease(const unsigned char* code,uint32_t) {
            HRESULT hr;
            if(d2vr_startup_shader::Word(code)==0xfffe0300u) {
                IDirect3DVertexShader9* shader=nullptr;
                hr=device->CreateVertexShader(reinterpret_cast<const DWORD*>(code),&shader);
                const bool good=SUCCEEDED(hr)&&shader;
                if(shader) shader->Release();
                return good;
            }
            IDirect3DPixelShader9* shader=nullptr;
            hr=device->CreatePixelShader(reinterpret_cast<const DWORD*>(code),&shader);
            const bool good=SUCCEEDED(hr)&&shader;
            if(shader) shader->Release();
            return good;
        }
    } ops{dev};
    const auto start=GetTickCount64();
    const auto r=d2vr_startup_shader::Run(reinterpret_cast<const unsigned char*>(storage.data()),
                                        size_t(size.QuadPart),10000,ops);
    Log("[StartupShaderWarmup] valid=%d attempted=%u succeeded=%u failed=%u budgetStop=%d elapsedMs=%llu; driver reuse unverified",
        int(r.valid),r.attempted,r.succeeded,r.failed,int(r.timedOut),GetTickCount64()-start);
} catch(...) {
    Log("[StartupShaderWarmup] aborted C++ allocation/processing failure; original device path continues");
}
