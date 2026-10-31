#pragma once
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>

// Experimental ownership prerequisite, NOT connected to renderer admission.
// Install only on a newly returned, not-yet-published IDirect3DStateBlock9.
// Preserve the native pointer, native COM identity, every HRESULT/refcount and
// the six documented interface methods; no code/EXE/shared vtable is patched.
// Callbacks may invalidate a device epoch; they MUST NOT throw or release self.
namespace d2vr_stateblock_lifetime {
enum class Operation { Capture, Apply };
struct Observer {
    void (*before)(IDirect3DStateBlock9*,Operation)=nullptr;
    void (*after)(IDirect3DStateBlock9*,Operation,HRESULT)=nullptr;
    void (*escape)(IDirect3DStateBlock9*)=nullptr;
};
struct Entry {
    IDirect3DStateBlock9* identity=nullptr; // compared only; no COM ownership
    std::array<void*,6> original{},table{};
    Observer observer;
    std::atomic<uint64_t> generation{1};
    std::atomic<unsigned> captures{0};
    std::atomic<bool> alive{true},escaped{false};
};
inline SRWLOCK registryLock=SRWLOCK_INIT;
inline std::unordered_map<IDirect3DStateBlock9*,std::shared_ptr<Entry>> registry;
constexpr size_t kCapacity=4096;
class Guard {
public:
    Guard(){AcquireSRWLockExclusive(&registryLock);}
    ~Guard(){ReleaseSRWLockExclusive(&registryLock);}
};
inline std::shared_ptr<Entry> Lookup(IDirect3DStateBlock9* self) {
    Guard lock;
    const auto it=registry.find(self);
    return it==registry.end()?std::shared_ptr<Entry>{}:it->second;
}
struct Version {
    std::shared_ptr<Entry> entry;
    uint64_t generation=0;
    bool Current()const {
        return entry && entry->alive.load(std::memory_order_acquire) &&
            !entry->escaped.load(std::memory_order_acquire) &&
            !entry->captures.load(std::memory_order_acquire) &&
            entry->generation.load(std::memory_order_acquire)==generation;
    }
};
inline Version Observe(IDirect3DStateBlock9* self) {
    auto entry=Lookup(self);
    if(!entry)return {};
    const auto generation=entry->generation.load(std::memory_order_acquire);
    Version result{std::move(entry),generation};
    return result.Current()?result:Version{};
}
using Query=HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*,REFIID,void**);
using Release=ULONG(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
using Call=HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
inline HRESULT STDMETHODCALLTYPE QueryHook(IDirect3DStateBlock9* self,REFIID iid,void** out) {
    const auto entry=Lookup(self);
    if(!entry)return D3DERR_INVALIDCALL;
    const HRESULT result=reinterpret_cast<Query>(entry->original[0])(self,iid,out);
    // Never change a successful output, IID, pointer, AddRef or HRESULT.
    // A distinct alias/unknown successful interface cannot certify ownership.
    if(SUCCEEDED(result) && out && *out &&
       (*out!=self || (!IsEqualGUID(iid,__uuidof(IUnknown)) &&
                      !IsEqualGUID(iid,__uuidof(IDirect3DStateBlock9))))) {
        // An alias can mutate the native object outside this shadow table.
        // Invalidating only an optional renderer observer would leave already
        // retained replay versions falsely current. The entry owns this denial.
        entry->escaped.store(true,std::memory_order_release);
        entry->generation.fetch_add(1,std::memory_order_acq_rel);
        if(entry->observer.escape)entry->observer.escape(self);
    }
    return result;
}
inline ULONG STDMETHODCALLTYPE ReleaseHook(IDirect3DStateBlock9* self) {
    const auto entry=Lookup(self);
    if(!entry)return 0; // valid callers must hold a native reference
    const ULONG refs=reinterpret_cast<Release>(entry->original[2])(self);
    if(!refs) {
        entry->alive.store(false,std::memory_order_release);
        entry->generation.fetch_add(1,std::memory_order_acq_rel);
        Guard lock;
        const auto it=registry.find(self);
        if(it!=registry.end() && it->second==entry)registry.erase(it);
        // Do not touch self: native Release has destroyed it. The local entry
        // keeps the shadow table alive until this thunk returns.
    }
    return refs;
}
inline HRESULT Invoke(IDirect3DStateBlock9* self,Operation op) {
    const auto entry=Lookup(self);
    if(!entry)return D3DERR_INVALIDCALL;
    const bool capture=op==Operation::Capture;
    if(capture) {
        entry->captures.fetch_add(1,std::memory_order_acq_rel);
        entry->generation.fetch_add(1,std::memory_order_acq_rel);
    }
    if(entry->observer.before)entry->observer.before(self,op);
    // Registry bookkeeping never holds a lock across native code/callbacks.
    const HRESULT result=reinterpret_cast<Call>(entry->original[capture?4:5])(self);
    if(capture) {
        entry->generation.fetch_add(1,std::memory_order_acq_rel);
        entry->captures.fetch_sub(1,std::memory_order_release);
    }
    if(entry->observer.after)entry->observer.after(self,op,result);
    return result;
}
inline HRESULT STDMETHODCALLTYPE CaptureHook(IDirect3DStateBlock9* self){return Invoke(self,Operation::Capture);}
inline HRESULT STDMETHODCALLTYPE ApplyHook(IDirect3DStateBlock9* self){return Invoke(self,Operation::Apply);}
// Isolated SEH boundary: a failed data-pointer exchange leaves the native
// object untouched. No VirtualProtect or executable instruction write.
inline bool Exchange(IDirect3DStateBlock9* self,void** expected,void** replacement) {
    __try {
        return InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(self),
            replacement,expected)==expected;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool PinCode(const void* address) {
    if(!address)return true;
    HMODULE module=nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(address),&module)!=FALSE;
}
inline bool Install(IDirect3DStateBlock9* self,Observer observer={}) {
    if(!self)return false;
    // A shadow vptr must never outlive executable thunk/observer storage.
    // Pin is process lifetime, not a Reset/unload-time reference to release.
    if(!PinCode(reinterpret_cast<const void*>(&ApplyHook)) ||
       !PinCode(reinterpret_cast<const void*>(observer.before)) ||
       !PinCode(reinterpret_cast<const void*>(observer.after)) ||
       !PinCode(reinterpret_cast<const void*>(observer.escape)))return false;
    try {
        auto entry=std::make_shared<Entry>();
        entry->identity=self;entry->observer=observer;
        auto original=*reinterpret_cast<void***>(self);
        for(size_t i=0;i<entry->table.size();++i)entry->original[i]=entry->table[i]=original[i];
        entry->table[0]=reinterpret_cast<void*>(&QueryHook);
        entry->table[2]=reinterpret_cast<void*>(&ReleaseHook);
        entry->table[4]=reinterpret_cast<void*>(&CaptureHook);
        entry->table[5]=reinterpret_cast<void*>(&ApplyHook);
        Guard lock;
        const auto old=registry.find(self);
        if(old!=registry.end())return original==old->second->table.data();
        if(registry.size()>=kCapacity)return false;
        registry.emplace(self,entry); // allocate BEFORE changing the vptr
        if(!Exchange(self,original,entry->table.data())) {registry.erase(self);return false;}
        return true;
    } catch(...) {return false;}
}
inline size_t Tracked(){Guard lock;return registry.size();}
// No Reset/Clear/uninstall: app-owned StateBlocks can outlive device Reset.
// Install pins the thunk/observer modules for the process lifetime.
// Versions detect Capture/Release; they are NOT a lock or a reusable snapshot.
// Replay must retain native COM refs and revalidate AFTER executing a stream.
// Untracked mutations, aliases, reentry or foreign threads still deny reuse.
}
