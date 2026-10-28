#pragma once
// Narrow x86 execution boundary for an owned, SHA-pinned instruction. No code,
// IAT or vtable writes. One debug slot on the calling thread, fail-closed when
// a debugger or another hardware breakpoint is present. Not a general profiler.
#include <windows.h>
#include <intrin.h>
#include <atomic>
#include "native_image_guard.h"

namespace d2vr_native_execution {
class Boundary {
public:
    using Callback=void(*)(const CONTEXT&);
private:
    inline static std::atomic<Boundary*> active_{nullptr};
    DWORD thread_=0;
    void* target_=nullptr;
    Callback callback_=nullptr;
    void* handler_=nullptr;
    bool installed_=false;
    DWORD previousDr0_=0,previousDr7_=0;
    struct Work {Boundary* owner;bool arm;bool ok=false;};
    static DWORD WINAPI ConfigureWorker(void* parameter) {
        auto& work=*static_cast<Work*>(parameter);auto& owner=*work.owner;
        HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME|
                                 THREAD_QUERY_LIMITED_INFORMATION,FALSE,owner.thread_);
        if(!thread)return 0;
        if(GetProcessIdOfThread(thread)!=GetCurrentProcessId()){CloseHandle(thread);return 0;}
        const DWORD prior=SuspendThread(thread);
        if(prior==DWORD(-1)){CloseHandle(thread);return 0;}
        if(prior!=0){ResumeThread(thread);CloseHandle(thread);return 0;}
        // Only kernel context operations while the caller is suspended: no
        // allocation, logging, loader calls, or waits on caller-owned locks.
        __try {
            CONTEXT context{};context.ContextFlags=CONTEXT_DEBUG_REGISTERS;
            if(GetThreadContext(thread,&context)) {
                if(work.arm) {
                    if(!context.Dr0 && !context.Dr1 && !context.Dr2 && !context.Dr3 &&
                       !(context.Dr7&0xffff20ffu)) {
                        owner.previousDr0_=context.Dr0;owner.previousDr7_=context.Dr7;
                        context.Dr0=reinterpret_cast<DWORD>(owner.target_);
                        context.Dr7|=1u; // slot0 local, execution, length1
                        context.Dr6&=~1u;
                        work.ok=SetThreadContext(thread,&context)!=FALSE;
                    }
                } else if(context.Dr0==reinterpret_cast<DWORD>(owner.target_) &&
                          (context.Dr7&0x000f0003u)==1u) {
                    context.Dr0=owner.previousDr0_;
                    context.Dr7=(context.Dr7&~0x000f0003u)|(owner.previousDr7_&0x000f0003u);
                    context.Dr6&=~1u;
                    work.ok=SetThreadContext(thread,&context)!=FALSE;
                }
            }
        } __finally {ResumeThread(thread);CloseHandle(thread);}
        return 0;
    }
    bool Configure(bool arm) {
        Work work{this,arm};
        HANDLE worker=CreateThread(nullptr,0,ConfigureWorker,&work,0,nullptr);
        if(!worker)return false;
        const DWORD waited=WaitForSingleObject(worker,INFINITE);
        CloseHandle(worker);return waited==WAIT_OBJECT_0 && work.ok;
    }
    static LONG CALLBACK Handle(EXCEPTION_POINTERS* exception) {
        auto owner=active_.load(std::memory_order_acquire);
        if(!owner || !exception || !exception->ExceptionRecord || !exception->ContextRecord ||
           exception->ExceptionRecord->ExceptionCode!=EXCEPTION_SINGLE_STEP ||
           exception->ExceptionRecord->ExceptionAddress!=owner->target_ ||
           GetCurrentThreadId()!=owner->thread_)return EXCEPTION_CONTINUE_SEARCH;
        auto& context=*exception->ContextRecord;
        if(context.Eip!=reinterpret_cast<DWORD>(owner->target_) ||
           context.Dr0!=context.Eip || (context.Dr7&0x000f0003u)!=1u ||
           (context.Dr6&0xe00fu)!=1u)return EXCEPTION_CONTINUE_SEARCH;
        const DWORD error=GetLastError();
        __declspec(align(16)) unsigned char floating[512];
        _fxsave(floating);
        __asm fninit
        owner->callback_(context);
        _fxrstor(floating);
        SetLastError(error);
        context.Dr6&=~1u;
        context.EFlags|=0x10000u; // RF: execute this original instruction once
        return EXCEPTION_CONTINUE_EXECUTION;
    }
public:
    Boundary()=default;
    Boundary(const Boundary&)=delete;
    Boundary& operator=(const Boundary&)=delete;
    bool Start(void* target,Callback callback,const d2vr_native_image::Guard& guard) {
        if(installed_)return target_==target && callback_==callback;
        if(!target || !callback || !guard.enabled || IsDebuggerPresent() ||
           reinterpret_cast<uintptr_t>(target)<guard.base ||
           reinterpret_cast<uintptr_t>(target)-guard.base>=guard.bytes)return false;
        MEMORY_BASIC_INFORMATION memory{};
        if(!VirtualQuery(target,&memory,sizeof(memory)) || memory.Type!=MEM_IMAGE ||
           memory.State!=MEM_COMMIT || memory.AllocationBase!=reinterpret_cast<void*>(guard.base) ||
           !(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ)) ||
           (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCSTR>(&Handle),&self))return false;
        Boundary* empty=nullptr;
        target_=target;callback_=callback;thread_=GetCurrentThreadId();
        if(!active_.compare_exchange_strong(empty,this))return false;
        handler_=AddVectoredExceptionHandler(1,Handle);
        if(handler_ && Configure(true)){installed_=true;return true;}
        active_=nullptr;
        if(handler_)RemoveVectoredExceptionHandler(handler_);
        handler_=nullptr;return false;
    }
    // Call on the same thread at a quiescent boundary. No asynchronous teardown
    // or destructor work against a thread that may be entering the handler.
    bool Stop() {
        if(!installed_)return true;
        if(GetCurrentThreadId()!=thread_ || !Configure(false))return false;
        installed_=false;active_=nullptr;
        const bool removed=RemoveVectoredExceptionHandler(handler_)!=0;
        handler_=nullptr;return removed;
    }
    bool Installed()const{return installed_;}
};
}
