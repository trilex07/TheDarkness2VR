// Included after Log/config and before the shader hooks. No native EXE writes.
#include "shader_preload_pool.h"
#include <process.h>
#include <memory>
#include <type_traits>

namespace d2vr_shader_preload {
struct State {
    IDirect3DDevice9* device; // identity only; unconsumed shaders own their COM refs
    Pool<IUnknown> pool;
    std::wstring path;
    std::atomic<uint64_t> requests{0},hits{0},saved{0},writeFailures{0},captureFailures{0};
    std::atomic<uint64_t> unknownMisses{0},depletedMisses{0},otherMisses{0};
    std::atomic<bool> writing{false};
    std::atomic<bool> usable{true};
    ULONGLONG nextTick=0;
    State(IDirect3DDevice9* dev,std::wstring file,unsigned copies=1):device(dev),pool(dev,copies),path(std::move(file)) {}
};
static std::atomic<State*> state{nullptr}; // process lifetime; never release COM under loader lock
struct ReleaseShader {void operator()(IUnknown* object)const {if(object)object->Release();}};

static bool Snapshot(const DWORD* source,std::vector<uint32_t>& code) {
    if(!source || (reinterpret_cast<uintptr_t>(source)&3)) return false;
    const auto base=reinterpret_cast<uintptr_t>(source);
    if(base>UINTPTR_MAX-d2vr_startup_shader::MaxWords*4) return false;
    code.clear();
    // Read only pages containing the shader, preserving PAGE_GUARD on failures.
    // No cached page permissions or delayed access to engine memory.
    while(code.size()<d2vr_startup_shader::MaxWords) {
        const size_t old=code.size();const uintptr_t at=base+old*4;
        MEMORY_BASIC_INFORMATION region{};
        if(VirtualQuery(reinterpret_cast<const void*>(at),&region,sizeof(region))!=sizeof(region) ||
           region.State!=MEM_COMMIT || (region.Protect&PAGE_GUARD)) return false;
        const DWORD access=region.Protect&0xff;
        if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
           access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return false;
        const size_t words=std::min<size_t>((4096-(at&4095))/4,d2vr_startup_shader::MaxWords-old);
        code.resize(old+words);SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),code.data()+old,words*4,&copied) || copied!=words*4) return false;
        if(code[0]!=0xfffe0300u && code[0]!=0xffff0300u) return false;
        for(size_t i=1;i<code.size();) {
            const auto token=code[i],op=token&0xffffu;
            if(op==0xffffu) {
                if(token!=0x0000ffffu) return false;
                code.resize(i+1);return true;
            }
            const size_t n=op==0xfffeu ? (token>>16)&0x7fffu : (token>>24)&15u;
            if(n>code.size()-i-1) break;
            i+=n+1;
        }
    }
    return false;
}
static std::vector<uint32_t> ReadArchive(const std::wstring& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size) || size.QuadPart<8 || size.QuadPart>d2vr_startup_shader::MaxBytes || size.QuadPart%4) {CloseHandle(file);return {};}
    std::vector<uint32_t> data;
    try {data.resize(size_t(size.QuadPart)/4);} catch(...) {CloseHandle(file);return {};}
    DWORD read=0;const BOOL ok=ReadFile(file,data.data(),DWORD(size.QuadPart),&read,nullptr);CloseHandle(file);
    if(!ok || read!=size.QuadPart || !ValidateArchive(reinterpret_cast<const unsigned char*>(data.data()),read)) return {};
    return data;
}
struct WriteJob {State* owner;std::wstring path;std::vector<uint32_t> data;uint64_t version;};
static unsigned __stdcall WriteArchive(void* value) {
    std::unique_ptr<WriteJob> job(static_cast<WriteJob*>(value));bool ok=false;
    try {
        const std::wstring pending=job->path+L".pending-"+std::to_wstring(GetCurrentProcessId());
        HANDLE file=CreateFileW(pending.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {
            DWORD written=0;const DWORD bytes=DWORD(job->data.size()*4);
            ok=WriteFile(file,job->data.data(),bytes,&written,nullptr) && written==bytes;
            CloseHandle(file);
            if(ok) ok=MoveFileExW(pending.c_str(),job->path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
            if(!ok) DeleteFileW(pending.c_str());
        }
    } catch(...) {ok=false;}
    if(ok) job->owner->saved.store(job->version);
    else ++job->owner->writeFailures;
    job->owner->writing.store(false);
    return 0;
}
static std::unique_ptr<State> PrepareState(IDirect3DDevice9* device,
    const std::wstring& path,const std::wstring& seed,unsigned copies=1,
    const std::wstring& legacy=L"") {
        copies=copies==2?2:1;
        auto s=std::make_unique<State>(device,path,copies);
        auto archive=ReadArchive(s->path);
        const bool alreadyPersisted=!archive.empty();
        if(archive.empty() && !legacy.empty()) archive=ReadArchive(legacy);
        if(archive.empty()) archive=ReadArchive(seed);
        unsigned ready=0,failed=0;bool budgetStop=false;
        const auto start=GetTickCount64();
        if(!archive.empty()) {
            // Cover every known bytecode once before spending the remaining
            // startup budget on a spare. Slower drivers retain broad coverage.
            const bool demand=archive[0]==DemandMagic;
            for(unsigned copy=0;copy<MaxCopies;++copy) {
            size_t offset=2;
            for(uint32_t i=0;i<archive[1];++i) {
                const unsigned quota=std::max(copies,demand?archive[offset++]:1u);
                const uint32_t words=archive[offset++];
                std::vector<uint32_t> code(archive.begin()+offset,archive.begin()+offset+words);offset+=words;
                if(!copy) s->pool.Remember(device,code,quota);
                if(copy>=quota) continue;
                if(failed) continue; // no repeated driver creation after rejection/device loss
                if(ready>=MaxObjects) {budgetStop=true;continue;}
                if(GetTickCount64()-start>=10000) {budgetStop=true;continue;}
                IUnknown* object=nullptr;HRESULT hr;
                if(code[0]==0xfffe0300u) {
                    IDirect3DVertexShader9* shader=nullptr;
                    hr=device->CreateVertexShader(reinterpret_cast<const DWORD*>(code.data()),&shader);object=shader;
                } else {
                    IDirect3DPixelShader9* shader=nullptr;
                    hr=device->CreatePixelShader(reinterpret_cast<const DWORD*>(code.data()),&shader);object=shader;
                }
                std::unique_ptr<IUnknown,ReleaseShader> owned(object);
                if(SUCCEEDED(hr) && object && s->pool.Adopt(device,code,object,hr)) {owned.release();++ready;}
                else ++failed;
            }
            }
        }
        // A menu/load/readiness delay must not discard precreated shaders before
        // the level requests them. Keep the bounded one-use inventory available.
        s->nextTick=GetTickCount64()+2000;
        if(alreadyPersisted) s->saved.store(s->pool.Version());
        Log("[ShaderPreload] startup ready=%u entries=%zu failed=%u budgetStop=%d elapsedMs=%llu; one-use owned objects, exact bytecode+device match",
            ready,s->pool.Size(),failed,int(budgetStop),GetTickCount64()-start);
        Log("[ShaderPreload] minimumCopies=%u learnedMaximum=%u maximumObjects=%u; independent creation references, no live refill",copies,MaxCopies,MaxObjects);
        return s;
}
static void Initialize(IDirect3DDevice9* device) {
    if(!g_startupShaderPool || !device || state.load()) return;
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Initialize),&pinned)) return;
    wchar_t base[MAX_PATH]{};
    if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,base))) return;
    try {
        const std::wstring directory=std::wstring(base)+L"\\Darkness2VR\\";
        // Keep the old archive intact for immediate rollback to older DLLs.
        state.store(PrepareState(device,directory+L"shader-demand.bin",directory+L"startup-shaders.bin",g_startupShaderCopies,
            directory+L"shader-preload.bin").release());
    } catch(...) {Log("[ShaderPreload] initialization failed; native creation retained");}
}

template<class Shader,class CreateFn>
static HRESULT Create(IDirect3DDevice9* device,const DWORD* code,Shader** out,CreateFn original) {
    State* s=state.load();
    if(!s || device!=s->device || !s->usable.load() || !out || !code) return original(device,code,out);
    ++s->requests;
    constexpr uint32_t kind=std::is_same<Shader,IDirect3DVertexShader9>::value ? 0xfffe0300u : 0xffff0300u;
    TakeStatus status=TakeStatus::Ineligible;
    try {
        std::vector<uint32_t> snapshot;
        if(Snapshot(code,snapshot)) {
            int32_t result=0;
            if(IUnknown* object=s->pool.Take(device,snapshot,kind,result,&status)) {
                *out=static_cast<Shader*>(object);++s->hits;return HRESULT(result);
            }
        }
    } catch(...) {} // unreadable/unsupported/allocation failure: use the original API
    if(status==TakeStatus::UnknownCode) ++s->unknownMisses;
    else if(status==TakeStatus::Depleted) ++s->depletedMisses;
    else ++s->otherMisses;
    const HRESULT hr=original(device,code,out);
    if(SUCCEEDED(hr) && *out) try {
        // Learn the effective, successfully created shader, including any mod
        // rewrite. Never persist engine pointers or infer identity from FNV.
        UINT bytes=0;
        if(SUCCEEDED((*out)->GetFunction(nullptr,&bytes)) && bytes>=8 && bytes<=32768 && !(bytes&3)) {
            std::vector<uint32_t> validated(bytes/4);
            if(SUCCEEDED((*out)->GetFunction(validated.data(),&bytes)) && bytes<=validated.size()*4 && !(bytes&3)) {
                validated.resize(bytes/4);
                if(!s->pool.Remember(device,validated)) ++s->captureFailures;
                else s->pool.ObserveCreation(device,validated);
            } else ++s->captureFailures;
        } else ++s->captureFailures;
    } catch(...) {++s->captureFailures;}
    return hr;
}
static void BeginReset(IDirect3DDevice9* device) {
    if(State* s=state.load()) if(s->device==device) {s->usable.store(false);s->pool.Enable(false);}
}
static void EndReset(IDirect3DDevice9* device,HRESULT result) {
    // Unlike default-pool surfaces, VS/PS objects survive Reset AND ResetEx.
    // Keep them through the game's initial Reset; never serve during/after a
    // failed Reset. No shader construction or binding in the reset callbacks.
    if(State* s=state.load()) if(s->device==device) {
        s->pool.Enable(SUCCEEDED(result));
        s->usable.store(SUCCEEDED(result));
        Log("[ShaderPreload] Reset complete hr=0x%08lX usable=%d; shader objects preserved",result,int(s->usable.load()));
    }
}
static void Tick(ULONGLONG now=GetTickCount64()) {
    State* s=state.load();if(!s) return;
    if(now<s->nextTick) return;s->nextTick=now+2000;
    Log("[ShaderPreload] requests=%llu hits=%llu learned=%zu saved=%llu writeFailures=%llu captureFailures=%llu pending=%zu unknownMisses=%llu depletedMisses=%llu otherMisses=%llu; no FPS claim",
        s->requests.load(),s->hits.load(),s->pool.Size(),s->saved.load(),s->writeFailures.load(),s->captureFailures.load(),s->pool.Pending(),
        s->unknownMisses.load(),s->depletedMisses.load(),s->otherMisses.load());
    if(!s->pool.Size() || s->saved.load()>=s->pool.Version() || s->writing.exchange(true)) return;
    try {
        auto job=std::make_unique<WriteJob>();job->owner=s;job->path=s->path;
        job->data=s->pool.Archive(job->version);
        auto handle=_beginthreadex(nullptr,0,&WriteArchive,job.get(),0,nullptr);
        if(handle) {job.release();CloseHandle(reinterpret_cast<HANDLE>(handle));}
        else {s->writing.store(false);++s->writeFailures;}
    } catch(...) {s->writing.store(false);++s->writeFailures;}
}
}
