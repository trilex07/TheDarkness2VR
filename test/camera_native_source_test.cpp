#define D2VR_MATCHED_CAPTURE 1
#include <windows.h>
#include <atomic>
#include <cstring>
#include <cassert>
#include <cstdio>
#include "native_view_instance.h"
static bool g_cameraNativeNoShake=true;
static d2vr_native_image::Guard g_nativeImageGuard;
static void Log(const char*,...){}
#include "camera_native_no_shake.inl"
alignas(16) static BYTE owner[0x1400],avatar[16],controller[0x80];
static DWORD controllerRef[3],avatarRef[3],cameraRef[3];
static unsigned originals=0,compositions=0;
static bool raiseInCompose=false;
static void __fastcall Original(void*,void*,void* ref) {
    ++originals;--static_cast<DWORD*>(ref)[1];
}
static void __fastcall Compose(void* self,void*,void* av,void* cam,const float* offset,float x,float y,float z) {
    assert(self==owner && av==avatarRef && cam==cameraRef);
    assert(avatarRef[1]==6 && cameraRef[1]==5);
    for(unsigned i=0;i<4;++i)assert(offset[i]==0);
    assert(x==0 && y==0 && z==0);
    for(unsigned i=0;i<16;++i)assert(owner[0x1f0+i]==0);
    for(unsigned i=0;i<12;++i)assert(owner[0x20c+i]==0);
    assert(owner[0x9bc]==0 && owner[0x1118]==0);
    // The event waveform is untouched at the composition boundary.
    assert(*reinterpret_cast<float*>(owner+0x12f0)==.75f);
    assert(*reinterpret_cast<float*>(owner+0xca0)==.5f);
    --avatarRef[1];--cameraRef[1];++compositions;
    if(raiseInCompose)RaiseException(0xE0001234,0,0,nullptr);
    for(unsigned i=0;i<64;++i)owner[0x70+i]=BYTE(i+1+compositions%123);
}
static bool useChannel=false;
static void __fastcall Owned(void* self,void*,void* ref) {
    if(useChannel){CameraSourceNoAnimation(self,nullptr,ref);return;}
    // Actual failing runtime path: ordinary composer updates +70 but does NOT
    // call the channel adapter or update model snapshot +100.
    ++avatarRef[1];
    alignas(16) const float zero[4]={};
    Compose(self,nullptr,avatarRef,ref,zero,0,0,0);
}
static void Setup() {
    std::memset(owner,0,sizeof(owner));std::memset(controller,0,sizeof(controller));
    *reinterpret_cast<DWORD*>(owner)=0xfdf170;
    *reinterpret_cast<DWORD*>(avatar)=0x1001630;
    *reinterpret_cast<DWORD*>(controller)=0xfe3ee8;
    controllerRef[0]=DWORD(reinterpret_cast<uintptr_t>(controller));
    avatarRef[0]=DWORD(reinterpret_cast<uintptr_t>(avatar));avatarRef[1]=5;avatarRef[2]=1;
    cameraRef[1]=5;cameraRef[2]=1;
    *reinterpret_cast<DWORD**>(owner+0x828)=controllerRef;
    *reinterpret_cast<DWORD**>(controller+0x74)=avatarRef;
    for(unsigned i=0;i<16;++i)owner[0x1f0+i]=BYTE(i+30);
    for(unsigned i=0;i<12;++i)owner[0x20c+i]=BYTE(i+60);
    owner[0x9bc]=owner[0x1118]=1;
    *reinterpret_cast<float*>(owner+0x12f0)=.75f;
    *reinterpret_cast<float*>(owner+0xca0)=.5f;
    g_cameraSourceAvatar.store(DWORD(reinterpret_cast<uintptr_t>(avatar)));
}
static void CheckRestored() {
    for(unsigned i=0;i<16;++i)assert(owner[0x1f0+i]==BYTE(i+30));
    for(unsigned i=0;i<12;++i)assert(owner[0x20c+i]==BYTE(i+60));
    assert(owner[0x9bc]==1 && owner[0x1118]==1);
}
static bool ExceptionRestores() {
    __try {CameraSourceDispatchBridge(owner,nullptr,cameraRef);}
    __except(GetExceptionCode()==0xE0001234?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
    return false;
}
int main() {
    g_cameraSourceOriginalDispatch=reinterpret_cast<CameraSourceDispatch>(Original);
    g_cameraSourceOwnedDispatch=reinterpret_cast<CameraSourceDispatch>(Owned);
    g_cameraSourceOwnedCompose=reinterpret_cast<CameraSourceCompose>(Compose);
    Setup();assert(!IsCameraSourceTable(0));
    for(unsigned i=0;i<10000;++i) {
        cameraRef[1]=5;CameraSourceDispatchBridge(owner,nullptr,cameraRef);
        CheckRestored();assert(avatarRef[1]==5 && cameraRef[1]==4);
        assert(std::memcmp(owner+0x70,owner+0x100,64)==0);
    }
    assert(compositions==10000 && originals==0);
    assert(g_cameraSourceNoAnimationCalls.load()==0);
    Setup();useChannel=true;CameraSourceDispatchBridge(owner,nullptr,cameraRef);CheckRestored();
    assert(std::memcmp(owner+0x70,owner+0x100,64)==0);
    assert(g_cameraSourceNoAnimationCalls.load()==1);useChannel=false;
    for(unsigned flag:{0x10afu,0x8a4u,0x10adu,0x10aeu}) {
        Setup();owner[flag]=1;CameraSourceDispatchBridge(owner,nullptr,cameraRef);CheckRestored();
    }
    Setup();*reinterpret_cast<DWORD*>(controller)=0;CameraSourceDispatchBridge(owner,nullptr,cameraRef);
    Setup();g_cameraSourceAvatar.store(0);CameraSourceDispatchBridge(owner,nullptr,cameraRef);
    assert(originals==6 && compositions==10001);
    Setup();raiseInCompose=true;assert(ExceptionRestores());CheckRestored();
    DWORD flags=0,a=0,c=0;
    __asm {
        mov eax,0x11223344
        mov ecx,0x55667788
        call CameraSourceSkipBone
        mov a,eax
        mov c,ecx
        pushfd
        pop flags
    }
    assert(a==0x11223344 && c==0x55667788 && (flags&0x8d5)==0x44);
    puts("PASS native source bridge: 10000 x86 calls, reference ownership/stack ABI, restoration including SEH, alternate-camera refusal, raw haptics, compare flags");
}
