#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <initializer_list>
#include "../third_party/minhook/include/MinHook.h"
#include "../src/native_image_guard.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);ExitProcess(1);}}while(0)
static void Log(const char* format,...) {va_list ap;va_start(ap,format);vprintf(format,ap);puts("");va_end(ap);}
static DWORD g_renderThreadId=GetCurrentThreadId();
static d2vr_native_image::Guard g_nativeImageGuard;
static struct Present {bool Ready()const{return true;}} g_imagePresent;
static const BYTE wrapper[]={0x8b,0x44,0x24,0x04,0x6a,0x01,0x50,0xff,0x15,0x58,0xb3,0xee,0x00,0xc3};
static bool VerifyEngineFrameCap(){return !memcmp(reinterpret_cast<void*>(0x998130),wrapper,sizeof(wrapper));}
#define D2VR_MATCHED_CAPTURE 1
#include "../src/native_sleep_timing.inl"
static unsigned apcs=0;
static void CALLBACK Apc(ULONG_PTR value){apcs+=unsigned(value);}
#pragma section(".lab",read,write)
__declspec(allocate(".lab")) volatile BYTE fixtureReserve[12*1024*1024]{};
int main(){
    setvbuf(stdout,nullptr,_IONBF,0);puts("Preparing exact wrapper ABI fixture");
    // Real x86 stdcall API and MinHook, with the exact admitted engine wrapper
    // and parent return layout reproduced in this test process only.
    // Own dedicated test-image padding reserves the historical addresses before
    // CRT/stack allocation. Never overwrite an unrelated reservation or image.
    fixtureReserve[0]=fixtureReserve[sizeof(fixtureReserve)-1]=1;
    for(uintptr_t base:{uintptr_t(0x990000),uintptr_t(0x930000),uintptr_t(0xee0000)})
        CHECK(base>=reinterpret_cast<uintptr_t>(fixtureReserve) &&
            base+65536<=reinterpret_cast<uintptr_t>(fixtureReserve)+sizeof(fixtureReserve));
    memcpy(reinterpret_cast<void*>(0x998130),wrapper,sizeof(wrapper));
    *reinterpret_cast<void**>(0xeeb358)=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"SleepEx"));
    BYTE caller[]={0xb8,1,0,0,0,0x50,0xe8,0,0,0,0,0x83,0xc4,0x04,0xc3};
    *reinterpret_cast<LONG*>(caller+7)=0x998130-0x938b67;
    memcpy(reinterpret_cast<void*>(0x938b5c),caller,sizeof(caller));
    DWORD old=0;CHECK(VirtualProtect(reinterpret_cast<void*>(0x990000),65536,PAGE_EXECUTE_READ,&old));
    CHECK(VirtualProtect(reinterpret_cast<void*>(0x930000),65536,PAGE_EXECUTE_READ,&old));
    CHECK(FlushInstructionCache(GetCurrentProcess(),nullptr,0));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(dos)+dos->e_lfanew);
    g_nativeImageGuard={true,reinterpret_cast<uintptr_t>(dos),nt->OptionalHeader.SizeOfImage};
    CHECK(WritePrivateProfileStringA("Diagnostics","NativeSleepTiming","1",".\\Darkness2VR-diagnostic.ini"));
    puts("Installing observer");PollNativeSleepTiming();CHECK(g_sleepTimingNext);
    using Caller=DWORD(__cdecl*)();auto invoke=reinterpret_cast<Caller>(0x938b5c);
    puts("Invoking admitted wrapper");SetLastError(0x1234);CHECK(invoke()==0);CHECK(GetLastError()==0x1234);
    CHECK(g_sleepWindow.count==1 && g_sleepWindow.deadline==1 && g_sleepWindow.requested==1);
    CHECK(QueueUserAPC(&Apc,GetCurrentThread(),3));
    CHECK(invoke()==WAIT_IO_COMPLETION && apcs==3);
    CHECK(g_sleepWindow.count==2 && g_sleepWindow.deadline==2 && g_sleepWindow.other==0 && g_sleepWindow.requested==2);
    const unsigned count=g_sleepWindow.count;CHECK(SleepEx(0,FALSE)==0);CHECK(g_sleepWindow.count==count);
    CHECK(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);CHECK(MH_Uninitialize()==MH_OK);
    CHECK(WritePrivateProfileStringA("Diagnostics","NativeSleepTiming",nullptr,".\\Darkness2VR-diagnostic.ini"));
    puts("PASS actual x86 SleepEx observer: exact wrapper/parent ABI, normal return, alertable APC delivery, LastError, unrelated caller exclusion; no game timing claim");
}
