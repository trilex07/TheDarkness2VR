#pragma once
// Intercept implementation entries, never replace a COM object or its vptr.
// Process-lifetime hooks: no raw object registry, no retained COM references.
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <mutex>
#include <utility>
#include <intrin.h>
#include "native_image_guard.h"
#include "present_overlap.h"
#include "../third_party/minhook/include/MinHook.h"

namespace d2vr_image_present {
enum class Method { Create, Query, Present };
using Present = HRESULT(STDMETHODCALLTYPE*)(IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD);
using Create = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9**);
using Query = HRESULT(STDMETHODCALLTYPE*)(IUnknown*,REFIID,void**);
struct Callbacks {
    bool (*qualify)(Method,uintptr_t)=nullptr;
    void (*before)(IDirect3DSwapChain9*,uintptr_t,bool,bool)=nullptr;
    void (*after)(IDirect3DSwapChain9*,uintptr_t,bool,bool,HRESULT)=nullptr;
    void (*log)(const char*,...)=nullptr;
    // Optional diagnostic timing brackets the actual outer native forwarding,
    // after before() and before after(). Both null in the ordinary fast path.
    void (*forwardBegin)()=nullptr;
    void (*forwardEnd)()=nullptr;
};

class Observer {
    static constexpr unsigned capacity=16;
    struct Entry {
        void* target=nullptr; Method kind{}; std::atomic<void*> next{nullptr};
        std::atomic<unsigned> receipts{0};
    } entries_[capacity];
    // Bound diagnostics per implementation and admission class. In particular,
    // an unrelated first caller must not consume the awaiting-create receipt.
    enum Receipt : unsigned { CreateOther=1,CreateNative=2,PresentOther=4,PresentAwaitCreate=8,PresentNative=16 };
    bool TakeReceipt(unsigned index,Receipt receipt) {
        if(!cb_.log) return false;
        auto& seen=entries_[index].receipts;const unsigned bit=receipt;
        return !(seen.load(std::memory_order_relaxed)&bit) &&
            !(seen.fetch_or(bit,std::memory_order_relaxed)&bit);
    }
    unsigned used_=0;
    std::recursive_mutex lock_;
    d2vr_native_image::Guard guard_;
    Callbacks cb_;
    std::atomic<bool> started_{false},created_{false},warm_{false},resetting_{false},fault_{false};
    std::atomic<DWORD> thread_{0};
    inline static std::atomic<Observer*> instance_{nullptr}; // pinned owner, never detached under DllMain
    struct Path {
        unsigned depth=0,indices[capacity]{}; void* objects[capacity]{};
        bool Push(unsigned index,void* object) {
            if(depth==capacity) return false;
            for(unsigned i=0;i<depth;++i) if(indices[i]==index && objects[i]==object) return false;
            indices[depth]=index;objects[depth++]=object;return true;
        }
    };
    inline static thread_local Path path_{};
    // The qualified, ready OUTER Present on this thread may be forwarded once
    // from inside before() (right-eye GPU wait overlap). Null everywhere else.
    struct Forward {
        Observer* self; Present next; IDirect3DSwapChain9* sc; const RECT* a; const RECT* b;
        HWND w; const RGNDATA* r; DWORD flags; bool measure;
        HRESULT operator()() const {
            ++self->forwarded;
            if(measure) self->cb_.forwardBegin();
            LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
            const HRESULT hr=next(sc,a,b,w,r,flags);
            QueryPerformanceCounter(&end);
            if(measure) self->cb_.forwardEnd();
            self->lastForwardTicks.store(uint64_t(end.QuadPart-begin.QuadPart),std::memory_order_relaxed);
            self->forwardSamples.fetch_add(1,std::memory_order_release);
            ++self->returned;
            return hr;
        }
    };
    struct Early { d2vr_present_overlap::Slot slot; const Forward* forward=nullptr; };
    inline static thread_local Early* early_=nullptr;
    template<unsigned I> static __declspec(noinline) HRESULT STDMETHODCALLTYPE CreateThunk(
        IDirect3DDevice9* d,D3DPRESENT_PARAMETERS* p,IDirect3DSwapChain9** s) {
        return instance_.load(std::memory_order_acquire)->OnCreate(I,d,p,s,reinterpret_cast<uintptr_t>(_ReturnAddress()));
    }
    template<unsigned I> static __declspec(noinline) HRESULT STDMETHODCALLTYPE QueryThunk(IUnknown* o,REFIID id,void** out) {
        return instance_.load(std::memory_order_acquire)->OnQuery(I,o,id,out);
    }
    template<unsigned I> static __declspec(noinline) HRESULT STDMETHODCALLTYPE PresentThunk(
        IDirect3DSwapChain9* s,const RECT* a,const RECT* b,HWND w,const RGNDATA* r,DWORD f) {
        return instance_.load(std::memory_order_acquire)->OnPresent(I,s,a,b,w,r,f,reinterpret_cast<uintptr_t>(_ReturnAddress()));
    }
    template<size_t... I> static void* Thunk(Method method,unsigned index,std::index_sequence<I...>) {
        const void* creates[]={reinterpret_cast<const void*>(&CreateThunk<I>)...};
        const void* queries[]={reinterpret_cast<const void*>(&QueryThunk<I>)...};
        const void* presents[]={reinterpret_cast<const void*>(&PresentThunk<I>)...};
        return const_cast<void*>((method==Method::Create?creates:method==Method::Query?queries:presents)[index]);
    }
    bool Qualifies(Method kind,uintptr_t caller) const {return cb_.qualify && cb_.qualify(kind,caller);}
    // MinHook writes target[0..4], or target[-5..1] for patchAbove. Admit a
    // conservative 80-byte readable/executable span covering decoding as well.
    bool PinExternalEntry(void* target) {
        const auto p=reinterpret_cast<uintptr_t>(target);
        if(p<5 || !guard_.enabled || !guard_.Allows(p-5,80)) return false;
        MEMORY_BASIC_INFORMATION first{},last{};
        if(!VirtualQuery(reinterpret_cast<void*>(p-5),&first,sizeof(first)) ||
           !VirtualQuery(reinterpret_cast<void*>(p+74),&last,sizeof(last))) return false;
        constexpr DWORD executable=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
        if(first.Type!=MEM_IMAGE || last.Type!=MEM_IMAGE || first.State!=MEM_COMMIT || last.State!=MEM_COMMIT ||
           first.AllocationBase!=last.AllocationBase || !(first.Protect&executable) || !(last.Protect&executable) ||
           ((first.Protect|last.Protect)&(PAGE_GUARD|PAGE_NOACCESS))) return false;
        HMODULE ours=nullptr,owner=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&PresentThunk<0>),&ours) || first.AllocationBase==ours) return false;
        return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(target),&owner)!=FALSE;
    }
    bool Install(void* target,Method kind) {
        std::lock_guard<std::recursive_mutex> held(lock_);
        for(unsigned i=0;i<used_;++i) if(entries_[i].target==target) {
            if(entries_[i].kind!=kind) {Fail("entry reused with incompatible ABI");return false;}
            return entries_[i].next.load(std::memory_order_acquire)!=nullptr;
        }
        if(used_==capacity || !PinExternalEntry(target)) {Fail("unadmitted implementation entry");return false;}
        const unsigned index=used_++;
        auto& entry=entries_[index];entry.target=target;entry.kind=kind;
        void* trampoline=nullptr;
        auto result=MH_CreateHook(target,Thunk(kind,index,std::make_index_sequence<capacity>{}),&trampoline);
        if(result!=MH_OK) {Fail("trampoline creation refused");return false;}
        // Publish BEFORE enabling: another caller can enter immediately.
        entry.next.store(trampoline,std::memory_order_release);
        result=MH_EnableHook(target);
        if(result!=MH_OK) {
            // The entry remains uninstrumented; no successful installation is
            // advertised. Keep storage/trampoline alive even on partial failure.
            Fail("entry enable refused");return false;
        }
        if(cb_.log) cb_.log("[ImagePresent] installed kind=%u target=%p trampoline=%p; original vptr/table retained",unsigned(kind),target,trampoline);
        return true;
    }
    bool Observe(IDirect3DSwapChain9* chain) {
        // Typed result of CreateAdditionalSwapChain or a successful known IID.
        // Read only the two PUBLIC slots used here; never infer table length.
        auto vt=*reinterpret_cast<void***>(chain);
        return Install(vt[0],Method::Query) && Install(vt[3],Method::Present);
    }
    HRESULT OnCreate(unsigned index,IDirect3DDevice9* dev,D3DPRESENT_PARAMETERS* pp,IDirect3DSwapChain9** out,uintptr_t caller) {
        auto next=reinterpret_cast<Create>(entries_[index].next.load(std::memory_order_acquire));
        const HRESULT hr=next(dev,pp,out);
        const bool native=Qualifies(Method::Create,caller);
        IDirect3DSwapChain9* chain=nullptr;bool observed=false;
        if(hr==D3D_OK && out && *out) {
            chain=*out;observed=Observe(chain);
            if(native) {created_=observed;warm_=false;thread_=0;}
        } else if(native) {created_=false;warm_=false;}
        if(cb_.log && native) cb_.log("[ImagePresent] additional returned hr=%08lX sc=%p caller=%p observed=%d",hr,chain,reinterpret_cast<void*>(caller),int(created_.load()));
        if(TakeReceipt(index,native?CreateNative:CreateOther))
            cb_.log("[ImagePresent] raw-create returned entry=%u target=%p dev=%p sc=%p caller=%p callerQualifies=%d observed=%d created=%d hr=%08lX thread=%lu",
                index,entries_[index].target,dev,chain,reinterpret_cast<void*>(caller),int(native),int(observed),int(created_.load()),hr,GetCurrentThreadId());
        return hr; // parameters/output/refcount/HRESULT belong to the native API
    }
    HRESULT OnQuery(unsigned index,IUnknown* object,REFIID id,void** out) {
        auto next=reinterpret_cast<Query>(entries_[index].next.load(std::memory_order_acquire));
        const HRESULT hr=next(object,id,out);
        if(hr==S_OK && out && *out && (IsEqualGUID(id,__uuidof(IDirect3DSwapChain9)) ||
            IsEqualGUID(id,__uuidof(IDirect3DSwapChain9Ex)))) Observe(static_cast<IDirect3DSwapChain9*>(*out));
        return hr; // unknown/private IIDs are transparently forwarded, untouched
    }
    HRESULT OnPresent(unsigned index,IDirect3DSwapChain9* sc,const RECT* a,const RECT* b,HWND w,const RGNDATA* r,DWORD flags,uintptr_t caller) {
        ++entries;
        if(!path_.Push(index,sc)) {++recursive;Fail("recursive implementation cycle");return D3DERR_INVALIDCALL;}
        struct Leave {Path& path;~Leave(){--path.depth;}} leave{path_};
        const bool outer=path_.depth==1;
        const bool callerQualified=outer && Qualifies(Method::Present,caller);
        const bool createdAtEntry=created_.load();
        const bool qualified=callerQualified && createdAtEntry;
        if(qualified) {
            const DWORD current=GetCurrentThreadId();DWORD expected=0;
            thread_.compare_exchange_strong(expected,current);
            if(thread_.load()!=current) Fail("native presentation changed thread without Reset");
        }
        const bool ready=qualified && Ready();
        auto next=reinterpret_cast<Present>(entries_[index].next.load(std::memory_order_acquire));
        const bool measureForward=qualified && cb_.forwardBegin && cb_.forwardEnd;
        const Forward forward{this,next,sc,a,b,w,r,flags,measureForward};
        Early early{};early.forward=&forward;early.slot.armed=outer && qualified && ready;
        Early* const previousEarly=early_;early_=&early;
        struct Restore {Early* previous;~Restore(){early_=previous;}} restoreEarly{previousEarly};
        if(outer && cb_.before) cb_.before(sc,caller,qualified,ready);
        early_=previousEarly; // no forward-early after before() returns
        // Exactly one original forward per outer call: early inside before(),
        // or here, with identical counters, timing brackets and HRESULT.
        const HRESULT hr=d2vr_present_overlap::Finish(early.slot,forward);
        if(hr==D3D_OK) ++ok;else if(FAILED(hr)) ++errors;else ++statuses;
        if(qualified) {
            ++nativePresents;
            if(hr!=D3D_OK) warm_=false;
            else if(!fault_ && !resetting_) warm_=true;
            if(FAILED(hr) && hr!=D3DERR_DEVICELOST && hr!=D3DERR_DEVICENOTRESET) Fail("native Present error");
        }
        // Receipt only AFTER the actual original return and counter update. No
        // inner overlay call may advertise an authoritative outer boundary.
        if(outer && TakeReceipt(index,!callerQualified?PresentOther:!createdAtEntry?PresentAwaitCreate:PresentNative))
            cb_.log("[ImagePresent] raw-present returned entry=%u target=%p sc=%p caller=%p callerQualifies=%d created=%d qualified=%d readyBefore=%d readyAfter=%d hr=%08lX thread=%lu entries=%llu forwarded=%llu returned=%llu native=%llu recursive=%llu ok=%llu statuses=%llu errors=%llu",
                index,entries_[index].target,sc,reinterpret_cast<void*>(caller),int(callerQualified),int(createdAtEntry),int(qualified),int(ready),int(Ready()),hr,GetCurrentThreadId(),
                entries.load(),forwarded.load(),returned.load(),nativePresents.load(),recursive.load(),ok.load(),statuses.load(),errors.load());
        if(outer && cb_.after) cb_.after(sc,caller,qualified,ready,hr);
        return hr;
    }
public:
    std::atomic<uint64_t> entries{0},forwarded{0},returned{0},recursive{0},ok{0},statuses{0},errors{0},nativePresents{0};
    // QPC ticks of the last original forward (any outer path) and its sequence.
    std::atomic<uint64_t> lastForwardTicks{0},forwardSamples{0};
    // Called only from inside before() of the qualified, ready outer native
    // Present on this thread. Returns false (nothing done) anywhere else.
    static bool ForwardEarly() {
        Early* early=early_;
        return early && early->forward && d2vr_present_overlap::Early(early->slot,*early->forward);
    }
    bool Start(IDirect3DDevice9* device,d2vr_native_image::Guard guard,Callbacks callbacks) {
        if(!device || !guard.enabled || instance_.load(std::memory_order_acquire)) return false;
        guard_=guard;cb_=callbacks;
        Observer* expected=nullptr;
        if(!instance_.compare_exchange_strong(expected,this,std::memory_order_release)) return false;
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(&PresentThunk<0>),&self)) {Fail("observer module pin refused");return false;}
        const auto status=MH_Initialize();
        if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) {Fail("hook library init refused");return false;}
        started_=Install((*reinterpret_cast<void***>(device))[13],Method::Create);
        return started_;
    }
    bool Ready() const {return started_ && created_ && warm_ && !fault_ && !resetting_;}
    // Event-only recovery may run before another successful Present, but only
    // on the verified native outer-call thread. This does NOT admit a frame.
    bool OwnsCurrentNativePresent() const {
        return started_ && created_ && !fault_ && !resetting_ && path_.depth==1 &&
            thread_.load()==GetCurrentThreadId();
    }
    bool Failed() const {return fault_;}
    bool InPresentation() const {return path_.depth!=0;}
    void Fail(const char* why) {fault_=true;warm_=false;if(cb_.log) cb_.log("[ImagePresent] FAIL %s; XR admission closed",why);}
    void ResetBegin() {resetting_=true;warm_=false;thread_=0;}
    void ResetEnd(bool success,IDirect3DDevice9* device=nullptr) {
        // Compatibility layers may replace public slot13 AFTER CreateDevice
        // returned. Re-observe its current implementation while the caller
        // supplies a live Reset receiver. The outer engine Create return must
        // still qualify; an inner wrapper call cannot grant XR admission.
        // No object/vtable writes, retained device pointer or per-frame probe.
        if(success && device && started_ && !fault_)
            Install((*reinterpret_cast<void***>(device))[13],Method::Create);
        resetting_=!success;
    }
};
}
