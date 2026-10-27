#pragma once
// Process-local USER32 implementation trampoline. It never changes the game's
// IAT, camera, COM object, resource descriptions or static class vtables.
#include <windows.h>
#include <atomic>
#include <intrin.h>
#include "source_quality.h"
#include "native_image_guard.h"
#include "../third_party/minhook/include/MinHook.h"

namespace d2vr_source_quality {
class ClientSize {
    using Query=BOOL(WINAPI*)(HWND,LPRECT);
    using Logger=void(*)(const char*,...);
    inline static std::atomic<ClientSize*> instance_{nullptr};
    std::atomic<Query> next_{nullptr};
    std::atomic<HWND> hwnd_{nullptr};
    std::atomic<uint64_t> expected_{0},reportedExpected_{0},reportedActual_{0};
    Mode mode_=Mode::Off;
    Size exact_{};
    uintptr_t caller_=0;
    Logger log_=nullptr;
    bool installed_=false;
    bool discoverWindow_=false;
    static __declspec(noinline) BOOL WINAPI Hook(HWND hwnd,LPRECT rect) {
        return instance_.load(std::memory_order_acquire)->QuerySize(hwnd,rect,reinterpret_cast<uintptr_t>(_ReturnAddress()));
    }
    BOOL QuerySize(HWND hwnd,LPRECT rect,uintptr_t caller) {
        // A successful Win32 return and last-error value remain authoritative.
        // Publish next_ before enabling so concurrent calls always forward once.
        const BOOL ok=next_.load(std::memory_order_acquire)(hwnd,rect);
        const DWORD error=GetLastError();
        // Factory creation precedes the engine's window constructor. Only the
        // verified native sizing call may bind its first process-owned HWND.
        // Never infer ownership from an arbitrary USER32 caller or failed call.
        if(discoverWindow_ && !hwnd_.load() && ok && rect && caller==caller_ && hwnd) {
            DWORD process=0;
            if(GetWindowThreadProcessId(hwnd,&process) && process==GetCurrentProcessId()) {
                HWND empty=nullptr;
                hwnd_.compare_exchange_strong(empty,hwnd);
            }
        }
        if(Eligible(ok!=FALSE,rect!=nullptr,reinterpret_cast<uintptr_t>(hwnd),
                    reinterpret_cast<uintptr_t>(hwnd_.load()),caller,caller_)) {
            if(rect->left==0 && rect->top==0 && rect->right>0 && rect->bottom>0) {
                const auto size=Choose(mode_,unsigned(rect->right),unsigned(rect->bottom),exact_);
                if(size.width) {
                    rect->right=LONG(size.width);rect->bottom=LONG(size.height);
                    expected_.store((uint64_t(size.width)<<32)|size.height);
                    ++requests;
                }
            }
        }
        SetLastError(error);
        return ok;
    }
    bool External(void* target,const d2vr_native_image::Guard& guard) {
        const auto p=reinterpret_cast<uintptr_t>(target);
        if(p<5 || !guard.enabled || !guard.Allows(p-5,80)) return false;
        MEMORY_BASIC_INFORMATION first{},last{};
        if(!VirtualQuery(reinterpret_cast<void*>(p-5),&first,sizeof(first)) ||
           !VirtualQuery(reinterpret_cast<void*>(p+74),&last,sizeof(last))) return false;
        constexpr DWORD exec=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
        if(first.Type!=MEM_IMAGE || last.Type!=MEM_IMAGE || first.State!=MEM_COMMIT || last.State!=MEM_COMMIT ||
           first.AllocationBase!=last.AllocationBase || !(first.Protect&exec) || !(last.Protect&exec) ||
           ((first.Protect|last.Protect)&(PAGE_NOACCESS|PAGE_GUARD))) return false;
        HMODULE ours=nullptr,owner=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&Hook),&ours) || first.AllocationBase==ours) return false;
        return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(target),&owner)!=FALSE;
    }
public:
    std::atomic<uint64_t> requests{0};
    bool Start(void* target,d2vr_native_image::Guard guard,Mode mode,HWND hwnd,uintptr_t caller,Logger log=nullptr,Size exact={},bool discoverWindow=false) {
        if(installed_) return true;
        if(instance_.load(std::memory_order_acquire) || (!hwnd && !discoverWindow) || !caller || mode==Mode::Off || unsigned(mode)>4 ||
           (mode==Mode::Exact && !Choose(mode,640,480,exact).width) || !External(target,guard)) return false;
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(&Hook),&self)) return false;
        const auto init=MH_Initialize();
        if(init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) return false;
        mode_=mode;exact_=exact;hwnd_=hwnd;caller_=caller;log_=log;discoverWindow_=discoverWindow;
        ClientSize* empty=nullptr;
        if(!instance_.compare_exchange_strong(empty,this,std::memory_order_release)) return false;
        void* trampoline=nullptr;
        if(MH_CreateHook(target,reinterpret_cast<void*>(&Hook),&trampoline)!=MH_OK) {instance_=nullptr;return false;}
        next_.store(reinterpret_cast<Query>(trampoline),std::memory_order_release);
        if(MH_EnableHook(target)!=MH_OK) {
            // No enabled candidate is advertised; retain trampoline/owner for
            // process lifetime rather than tearing down from an active callback.
            return false;
        }
        installed_=true;
        if(log_) log_("[SourceQuality] external USER32 observer mode=%u caller=%p target=%p; EXE/IAT unchanged; actual RT0 required",unsigned(mode_),reinterpret_cast<void*>(caller_),target);
        return true;
    }
    void Window(HWND hwnd) {if(hwnd_.exchange(hwnd)!=hwnd) expected_=0;}
    bool Installed() const {return installed_;}
    void Source(unsigned width,unsigned height,unsigned format) {
        // This is the final RT0 used by stereo capture, not proof that every
        // upstream G-buffer/postprocess source has the same dimensions.
        if(!installed_) return;
        const auto expected=expected_.load(),actual=(uint64_t(width)<<32)|height;
        if(!expected) {
            if(reportedActual_.exchange(actual)!=actual && log_)
                log_("[SourceQuality] outputRT0=%ux%u format=%u nativeRequests=%llu; native sizing not observed, resolution request UNCONFIRMED",
                    width,height,format,requests.load());
            return;
        }
        if(expected==reportedExpected_.load() && actual==reportedActual_.load()) return;
        reportedExpected_=expected;reportedActual_=actual;
        if(log_) log_("[SourceQuality] outputRT0=%ux%u format=%u requested=%ux%u match=%d nativeRequests=%llu; request is NOT resolution proof",
            width,height,format,unsigned(expected>>32),unsigned(expected),expected==actual,requests.load());
    }
};
}
