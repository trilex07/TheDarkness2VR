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
    const char* status_="idle";
    DWORD error_=0,protection_=0;
    DWORD debug_[5]{};
    bool Fail(const char* stage,DWORD error=0){status_=stage;error_=error;return false;}
    struct Work {
        Boundary* owner;bool arm;bool ok=false;
        const char* status="configure";DWORD error=0;DWORD debug[5]{};
    };
    static DWORD WINAPI ConfigureWorker(void* parameter) {
        auto& work=*static_cast<Work*>(parameter);auto& owner=*work.owner;
        HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME|
                                 THREAD_QUERY_LIMITED_INFORMATION,FALSE,owner.thread_);
        if(!thread){work.status="open-thread";work.error=GetLastError();return 0;}
        const DWORD process=GetProcessIdOfThread(thread);
        if(process!=GetCurrentProcessId()){
            work.status="owner-process";work.error=process?0:GetLastError();CloseHandle(thread);return 0;
        }
        const DWORD prior=SuspendThread(thread);
        if(prior==DWORD(-1)){work.status="suspend-thread";work.error=GetLastError();CloseHandle(thread);return 0;}
        if(prior!=0){work.status="already-suspended";ResumeThread(thread);CloseHandle(thread);return 0;}
        // Only kernel context operations while the caller is suspended: no
        // allocation, logging, loader calls, or waits on caller-owned locks.
        __try {
            CONTEXT context{};context.ContextFlags=CONTEXT_DEBUG_REGISTERS;
            if(GetThreadContext(thread,&context)) {
                work.debug[0]=context.Dr0;work.debug[1]=context.Dr1;
                work.debug[2]=context.Dr2;work.debug[3]=context.Dr3;work.debug[4]=context.Dr7;
                if(work.arm) {
                    work.status="occupied-debug-state";
                    if(!context.Dr0 && !context.Dr1 && !context.Dr2 && !context.Dr3 &&
                       !(context.Dr7&0xffff20ffu)) {
                        owner.previousDr0_=context.Dr0;owner.previousDr7_=context.Dr7;
                        context.Dr0=reinterpret_cast<DWORD>(owner.target_);
                        context.Dr7|=1u; // slot0 local, execution, length1
                        context.Dr6&=~1u;
                        work.ok=SetThreadContext(thread,&context)!=FALSE;
                        work.status=work.ok?"ready":"set-context";
                        if(!work.ok)work.error=GetLastError();
                    }
                } else if(context.Dr0==reinterpret_cast<DWORD>(owner.target_) &&
                          (context.Dr7&0x000f0003u)==1u) {
                    context.Dr0=owner.previousDr0_;
                    context.Dr7=(context.Dr7&~0x000f0003u)|(owner.previousDr7_&0x000f0003u);
                    context.Dr6&=~1u;
                    work.ok=SetThreadContext(thread,&context)!=FALSE;
                    work.status=work.ok?"stopped":"set-context";
                    if(!work.ok)work.error=GetLastError();
                } else work.status="changed-debug-state";
            } else {work.status="get-context";work.error=GetLastError();}
        } __finally {ResumeThread(thread);CloseHandle(thread);}
        return 0;
    }
    bool Configure(bool arm) {
        Work work{this,arm};
        HANDLE worker=CreateThread(nullptr,0,ConfigureWorker,&work,0,nullptr);
        if(!worker)return Fail("create-worker",GetLastError());
        const DWORD waited=WaitForSingleObject(worker,INFINITE);
        const DWORD error=waited==WAIT_FAILED?GetLastError():0;
        CloseHandle(worker);
        if(waited!=WAIT_OBJECT_0)return Fail("wait-worker",error);
        status_=work.status;error_=work.error;
        for(unsigned i=0;i<5;++i)debug_[i]=work.debug[i];
        return work.ok;
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
        if(installed_)return (target_==target && callback_==callback) || Fail("different-target");
        status_="validating";error_=0;protection_=0;
        for(auto& value:debug_)value=0;
        if(!target || !callback || !guard.enabled)return Fail("guard");
        if(IsDebuggerPresent())return Fail("debugger");
        if(reinterpret_cast<uintptr_t>(target)<guard.base ||
           reinterpret_cast<uintptr_t>(target)-guard.base>=guard.bytes)return Fail("target-range");
        MEMORY_BASIC_INFORMATION memory{};
        if(!VirtualQuery(target,&memory,sizeof(memory)))return Fail("virtual-query",GetLastError());
        protection_=memory.Protect;
        if(memory.Type!=MEM_IMAGE || memory.State!=MEM_COMMIT ||
           memory.AllocationBase!=reinterpret_cast<void*>(guard.base))return Fail("image-page");
        // This boundary observes execution without writing code. All executable
        // image protections are valid, including loader/driver writable copies.
        if(!(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) ||
           (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return Fail("page-protection");
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCSTR>(&Handle),&self))return Fail("pin-module",GetLastError());
        Boundary* empty=nullptr;
        target_=target;callback_=callback;thread_=GetCurrentThreadId();
        if(!active_.compare_exchange_strong(empty,this))return Fail("active-owner");
        handler_=AddVectoredExceptionHandler(1,Handle);
        if(!handler_)Fail("add-handler",GetLastError());
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
    const char* Status()const{return status_;}
    DWORD Error()const{return error_;}
    DWORD Protection()const{return protection_;}
    DWORD Debug(unsigned index)const{return index<5?debug_[index]:0;}
};
}
