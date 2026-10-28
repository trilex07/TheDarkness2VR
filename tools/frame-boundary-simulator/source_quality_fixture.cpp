#include <windows.h>
#include <intrin.h>
extern "C" __declspec(dllexport) __declspec(noinline) unsigned WINAPI FixturePadding(unsigned n) { volatile unsigned x=n; for(unsigned i=0;i<32;++i) x=x*1664525+1013904223; return x; }
static uintptr_t lastCaller=0;
static unsigned calls=0;
extern "C" __declspec(dllexport) __declspec(noinline) BOOL WINAPI SizeQuery(HWND window,LPRECT out) {
    ++calls;lastCaller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(out) *out={0,0,1920,1071};
    SetLastError(0x1234);
    return window!=reinterpret_cast<HWND>(2) && out ? TRUE:FALSE;
}
extern "C" __declspec(dllexport) __declspec(noinline) BOOL WINAPI SizeInvoke(HWND window,LPRECT out) {
    volatile BOOL result=SizeQuery(window,out);return result;
}
extern "C" __declspec(dllexport) __declspec(noinline) BOOL WINAPI OtherInvoke(HWND window,LPRECT out) {
    volatile BOOL result=SizeQuery(window,out);return result;
}
extern "C" __declspec(dllexport) __declspec(noinline) BOOL WINAPI ResourceInvoke(HWND window,LPRECT out) {
    volatile BOOL result=SizeQuery(window,out);return result;
}
extern "C" __declspec(dllexport) uintptr_t WINAPI SizeCaller() {return lastCaller;}
extern "C" __declspec(dllexport) unsigned WINAPI SizeCalls() {return calls;}
