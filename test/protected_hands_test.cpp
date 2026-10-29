#define NOMINMAX
#define D2VR_MATCHED_CAPTURE
#include <windows.h>
#include <openxr/openxr.h>
#include <intrin.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstdlib>
#include "../src/vr_input.h"
#include "../src/native_aim_math.h"
#include "../src/native_view_instance.h"
#include "../src/native_view_code_clone.h"
#include "actual_hands_types.inl"
static unsigned checks=0,originalCalls=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static void Log(const char*,...){}
static ULONGLONG clockMs=1000;
static bool g_nativeCinematicAdmitted=true,g_nativeCinematicPlaying=false;
static ULONGLONG g_nativeCinematicFrame=0;
static ULONGLONG Clock(){return clockMs;}
static bool g_nativeHandsEnabled=false,g_nativeHandsDirtyBoth=false,g_nativeHandsPackInstalled=false;
static bool g_restoredTrackedGameplay=false,g_protectedTrackedHands=true,g_nativePlayerOwnerAdmitted=true;
static bool g_imagePreservingInput=true,g_protectedGestureActions=true,g_haveViews=true,g_in3DGameplayMode=true;
static bool g_headGeometry=true,g_headGeometryReferenceValid=true;
static float g_gameUnitsPerMeter=1;
static DWORD g_renderThreadId=GetCurrentThreadId();
static d2vr_native_image::Guard g_nativeImageGuard{true,0x400000,0x100000};
static struct {uint64_t id=20;bool open=true;XrViewStateFlags flags=15;} g_trackedFrame;
#include "actual_cinematic_gate.inl"
static XrPosef g_headGeometryReference{{0,0,0,1},{}};
static d2vr_input::Tentacles g_protectedLeftTentacle,g_protectedRightTentacle;
static struct {bool gripValid=true;XrPosef grip{{0,0,0,1},{}};} g_hands[2];
static bool equippedAim[2]{};
static XrPosef weaponAim[2]{};
static bool CopyProtectedWeaponHandPose(DWORD,unsigned hand,XrPosef& pose) {
    if(!equippedAim[hand])return false;
    pose=weaponAim[hand];return true;
}
static SRWLOCK g_nativeWeaponLock=SRWLOCK_INIT,g_nativeGameplayLock=SRWLOCK_INIT;
static NativeWeaponPose g_nativeWeaponPoses[2];
static bool g_nativeSupport=false;
static bool g_nativeWeaponInstalled=false;
static NativeGameplaySample g_nativeGameplay;
static PVOID volatile g_nativeHandsSelectedResource=nullptr;
static __declspec(align(16)) BYTE camera[0x200],avatar[0x2000],inventory[0x400],parent[0x200],actor[0x20],renderer[0x600],skeleton[0x100];
static BYTE* resource=nullptr;
static void* references[20];static unsigned nextRef=0;
static void* sourceTable[96];
static struct {const char* name;void* unused[2];} names[130];
static __declspec(align(16)) float palette[130][16],pristine[130][16];
using SkinPack=void(__cdecl*)(float*,const float*,const float*);
static SkinPack nativeSkinPack=nullptr;
static __declspec(align(16)) float bindPose[130][16],packed[34][12];
static DWORD Ptr(const void* p){return DWORD(reinterpret_cast<uintptr_t>(p));}
static void Word(BYTE* p,unsigned o,DWORD v){memcpy(p+o,&v,4);}
static void Ref(BYTE* p,unsigned o,void* value){references[nextRef]=value;*reinterpret_cast<void***>(p+o)=&references[nextRef++];}
static bool ProtectedAimCameraOwned(void* p){return p==camera;}
static bool TryNativeWeaponMatrix(void*,float*,DWORD){return false;}
static bool NativeImageWriteAllowed(uintptr_t,size_t){return false;}
#define GetTickCount64 Clock
#include "actual_hands_owner.inl"
#include "../src/native_hands.inl"
#include "../src/protected_hands.inl"
#undef GetTickCount64
static void Matrix(float* m,float x=0,float y=0,float z=0){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;m[12]=x;m[13]=y;m[14]=z;}
static void Reset(){
    clockMs=1000;nextRef=0;originalCalls=0;g_gameUnitsPerMeter=1;
    memset(camera,0,sizeof(camera));memset(avatar,0,sizeof(avatar));memset(inventory,0,sizeof(inventory));
    memset(parent,0,sizeof(parent));memset(actor,0,sizeof(actor));memset(renderer,0,sizeof(renderer));
    memset(resource,0,0x620);memset(skeleton,0,sizeof(skeleton));
    g_nativeGameplay={};g_nativeGameplay.camera=Ptr(camera);g_nativeGameplay.inventory=Ptr(inventory);
    g_nativeGameplay.time=clockMs;g_nativeGameplay.grab=d2vr_input::GrabFeedback::Empty;
    g_nativeGameplay.avatarSkinResource=Ptr(resource);g_nativeHandsSelectedResource=resource;
    Word(camera,0,0xFFC7C0);Word(avatar,0,0x1001630);Word(inventory,0,0xFFCA10);
    Word(parent,0,0xFFC300);Word(actor,0,0xFC5F30);Word(skeleton,0,0xFC2338);
    Ref(camera,0x30,avatar);Ref(avatar,0x62C,camera);Ref(avatar,0x630,inventory);Ref(inventory,0x2A8,avatar);
    Ref(avatar,0x1F8C,parent);Ref(parent,0xF8,actor);Ref(actor,8,renderer);Ref(renderer,0x5BC,resource);Ref(resource,0x5E0,skeleton);
    Word(skeleton,0x90,Ptr(names));Word(skeleton,0x94,Ptr(names+130));
    for(auto& name:names)name.name="other";
    names[5].name="GAME_L1_ARM3";names[27].name="GAME_R1_ARM3";
    names[21].name="GAME_L1_WEAPON1";names[43].name="GAME_R1_WEAPON1";
    Matrix(reinterpret_cast<float*>(camera+0x80));Matrix(reinterpret_cast<float*>(parent+0x140));
    for(auto& bone:palette)Matrix(bone);
    Matrix(palette[3],-.15f);Matrix(palette[4],-.45f);Matrix(palette[5],-.75f);
    Matrix(palette[21],-.75f);Matrix(palette[22],-.45f);Matrix(palette[23],-.15f);
    Matrix(palette[25],.15f);Matrix(palette[26],.45f);Matrix(palette[27],.75f);
    Matrix(palette[43],.75f);Matrix(palette[44],.45f);Matrix(palette[45],.15f);
    memcpy(pristine,palette,sizeof(pristine));
    for(unsigned i=0;i<3;++i){Word(renderer,0x3F4+16*i,Ptr(palette));Word(renderer,0x3F8+16*i,Ptr(palette+130));}
    g_nativeHandCalibration={};for(auto& cache:g_nativeHandsCache)cache={};g_nativeHandsLastWrite={};
    g_nativeCinematicAdmitted=true;g_nativeCinematicPlaying=false;g_nativeCinematicFrame=20;
    g_nativeHandsWriteSeq=0;g_trackedFrame.id=20;g_trackedFrame.open=true;g_trackedFrame.flags=15;
    g_protectedTrackedHands=g_haveViews=g_in3DGameplayMode=g_imagePreservingInput=g_protectedGestureActions=true;
    g_nativeImageGuard.enabled=g_headGeometry=g_headGeometryReferenceValid=true;
    g_protectedRightTentacle.paused=g_protectedLeftTentacle.paused=false;
    g_renderThreadId=GetCurrentThreadId();g_headGeometryReference={{0,0,0,1},{}};
    g_hands[0].grip={{0,0,0,1},{-.5f,.2f,-.1f}};g_hands[1].grip={{0,0,0,1},{.5f,.2f,-.1f}};
    g_hands[0].gripValid=g_hands[1].gripValid=true;ResetProtectedHandPoses();
    equippedAim[0]=equippedAim[1]=false;
    *reinterpret_cast<void***>(resource)=sourceTable;
    CHECK(g_protectedHandsTable.Publish(resource,g_nativeImageGuard));
}
static bool Apply(){PublishProtectedHandPoses();return ApplyNativeHandsChecked(renderer,0xFFC7C0,resource,true);}
static bool Near(float a,float b){return std::fabs(a-b)<.0001f;}
static void Untouched(unsigned first,unsigned end){for(unsigned i=first;i<end;++i)CHECK(!memcmp(palette[i],pristine[i],64));}
static void PackFixtureMesh() {
    // Fixture remaps deliberately differ from bone order. Native6C3890 selects
    // a mesh bind ID, maps it through renderer+5A8, then passes the pose and
    // resource+9C bind matrix to B1E500. No real mesh membership is inferred.
    auto* selected=*reinterpret_cast<float**>(renderer+0x3F4+16**reinterpret_cast<DWORD*>(renderer+0x424));
    for(unsigned j=0;j<34;++j) {
        const unsigned bone=j<16?3+j:j<32?25+j-16:j==32?0:46;
        const unsigned bindId=129-bone;
        nativeSkinPack(packed[j],selected+bone*16,bindPose[bindId]);
        float expected[16];d2vr_math::Multiply(bindPose[bindId],selected+bone*16,expected);
        for(unsigned column=0;column<3;++column)for(unsigned row=0;row<4;++row)
            CHECK(Near(packed[j][column*4+row],expected[row*4+column]));
    }
}
static void NativePackOracle(const char* path) {
    FILE* f=nullptr;CHECK(!fopen_s(&f,path,"rb")&&f);
    auto* code=static_cast<BYTE*>(VirtualAlloc(nullptr,0x141,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));CHECK(code);
    CHECK(fread(code,1,0x141,f)==0x141&&fgetc(f)==EOF);fclose(f);
    DWORD old=0;CHECK(VirtualProtect(code,0x141,PAGE_EXECUTE_READ,&old));CHECK(FlushInstructionCache(GetCurrentProcess(),code,0x141));
    nativeSkinPack=reinterpret_cast<SkinPack>(code);
    for(unsigned i=0;i<130;++i) {
        Matrix(bindPose[i],float(i%7)/16,float(i%5)/16,-float(i%3)/16);
        if(i%2){bindPose[i][0]=bindPose[i][5]=0;bindPose[i][1]=1;bindPose[i][4]=-1;}
    }
    for(unsigned pose=0;pose<3;++pose)for(unsigned skin=0;skin<2;++skin) {
        Reset();Word(renderer,0x424,pose);Word(renderer,0x448,skin);CHECK(Apply());PackFixtureMesh();
        float before[34][12];memcpy(before,packed,sizeof(before));
        g_hands[0].grip.position.y+=.125f;CHECK(Apply());PackFixtureMesh();
        CHECK(memcmp(before,packed,16*48));CHECK(!memcmp(before+16,packed+16,18*48));
        memcpy(before,packed,sizeof(before));
        g_hands[1].grip.position.y+=.125f;CHECK(Apply());PackFixtureMesh();
        CHECK(!memcmp(before,packed,16*48));CHECK(memcmp(before+16,packed+16,16*48));
        CHECK(!memcmp(before+32,packed+32,2*48));memcpy(before,packed,sizeof(before));
        for(unsigned n=0;n<20;++n){CHECK(Apply());PackFixtureMesh();CHECK(!memcmp(before,packed,sizeof(before)));}
    }
    nativeSkinPack=nullptr;CHECK(VirtualFree(code,0,MEM_RELEASE));
    puts("PASS exact private B1E500 arithmetic: remapped nonidentity bind*pose, packed columns, independent peer output, six rotating slot combinations; fixture mesh only, NOT real GPU draw membership");
}
static unsigned expectedMxcsr=0;static unsigned short expectedStatus=0;static bool recursive=false;
static void __fastcall Original(void* self,void*,void* argument){
    CHECK(self==resource);++originalCalls;
    CHECK(_mm_getcsr()==expectedMxcsr);unsigned short status=0;__asm fnstsw status
    CHECK(status==expectedStatus);CHECK(GetLastError()==0x12345678);
    auto ref=static_cast<DWORD*>(argument);CHECK(ref[0]==Ptr(renderer) && ref[1]==1);--ref[1];
    if(recursive){recursive=false;DWORD nested[3]={Ptr(renderer),1,0};ProtectedHandsPackHook(self,nullptr,nested);CHECK(nested[1]==0);}
    SetLastError(0x87654321);
}
template<class F>static void Reject(F change){Reset();change();CHECK(!Apply());CHECK(!memcmp(palette,pristine,sizeof(palette)));}
int main(int argc,char** argv){
    CHECK(argc==1||argc==2);
    resource=static_cast<BYTE*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,0x620));CHECK(resource);
    for(auto& entry:sourceTable)entry=reinterpret_cast<void*>(&Original);
    CHECK(g_protectedHandsTable.Prepare(sourceTable,96,0xEC/4,reinterpret_cast<void*>(&ProtectedHandsPackHook),g_nativeImageGuard));
    g_protectedHandsPrivateReady=true;
    Reset();CHECK(Apply());CHECK(Near(palette[5][12],-.5f)&&Near(palette[27][12],.5f));
    CHECK(Near(palette[5][13],.2f)&&Near(palette[27][14],.1f));
    Reset();g_gameUnitsPerMeter=.5f;CHECK(Apply());
    CHECK(Near(palette[5][12],-.25f)&&Near(palette[27][12],.25f));
    CHECK(Near(palette[5][13],.1f)&&Near(palette[27][14],.05f));
    Untouched(0,2);Untouched(46,130);
    Reject([]{g_gameUnitsPerMeter=0;});Reject([]{g_gameUnitsPerMeter=NAN;});
    Reject([]{g_gameUnitsPerMeter=5001;});
    Reset();CHECK(Apply());
    Untouched(0,2);Untouched(46,130);CHECK(renderer[0x580] && renderer[0x581]);
    CHECK(!g_nativeHandsEnabled && !g_nativeHandsInstalled && !g_nativeGameplay.entities[0]);
    Reset();g_hands[1].gripValid=false;CHECK(Apply());CHECK(Near(palette[5][12],-.5f));Untouched(24,130);
    Reset();g_hands[0].gripValid=false;CHECK(Apply());CHECK(Near(palette[27][12],.5f));Untouched(0,24);Untouched(46,130);
    Reset();g_hands[1].grip.position.x=5;CHECK(Apply());CHECK(Near(palette[5][12],-.5f));Untouched(24,130);
    Reset();g_hands[0].grip.position.x=5;CHECK(Apply());CHECK(Near(palette[27][12],.5f));Untouched(0,24);
    Reset();g_hands[1].grip.orientation.w=NAN;CHECK(Apply());CHECK(Near(palette[5][12],-.5f));Untouched(24,130);
    // A bad native wrist/socket/finger on one side cannot suppress its peer.
    for(unsigned bone:{3u,5u,6u,21u,25u,27u,28u,43u}) {
        Reset();palette[bone][0]=NAN;float before[22][16];
        const unsigned first=bone<24?2:24;memcpy(before,palette+first,sizeof(before));
        CHECK(Apply());CHECK(!memcmp(before,palette+first,sizeof(before)));
        CHECK(Near(palette[bone<24?27:5][12],bone<24?.5f:-.5f));
    }
    // Each calibration can initialize and recover without the peer socket.
    Reset();palette[43][0]=NAN;CHECK(Apply());
    CHECK(g_nativeHandCalibration.leftValid&&!g_nativeHandCalibration.rightValid);
    memcpy(palette[43],pristine[43],64);CHECK(Apply());CHECK(g_nativeHandCalibration.rightValid);
    Reset();palette[21][0]=NAN;CHECK(Apply());
    CHECK(g_nativeHandCalibration.rightValid&&!g_nativeHandCalibration.leftValid);
    memcpy(palette[21],pristine[21],64);CHECK(Apply());CHECK(g_nativeHandCalibration.leftValid);
    for(unsigned pose=0;pose<3;++pose)for(unsigned skin=0;skin<2;++skin) {
        Reset();Word(renderer,0x424,pose);Word(renderer,0x448,skin);CHECK(Apply());
        CHECK(g_nativeHandsLastWrite.poseSlot==pose && g_nativeHandsLastWrite.skinSlot==skin);
        CHECK(renderer[0x580] && renderer[0x581]);
    }
    // Repeated packing must solve from the cached pristine pose, without drift.
    Reset();CHECK(Apply());float once[130][16];memcpy(once,palette,sizeof(once));
    for(unsigned n=0;n<100;++n){CHECK(Apply());CHECK(!memcmp(once,palette,sizeof(once)));}
    // A one-sided native animation update must preserve the pristine peer.
    for(unsigned hand=0;hand<2;++hand) {
        Reset();CHECK(Apply());const unsigned offset=hand*22,other=(1-hand)*22;
        memcpy(palette+2+offset,pristine+2+offset,22*64);
        palette[2+offset][12]=.03125f;
        float nativeUpdated[22][16];memcpy(nativeUpdated,palette+2+offset,sizeof(nativeUpdated));
        CHECK(Apply());
        CHECK(!memcmp(g_nativeHandsCache[0].original+other,pristine+2+other,22*64));
        CHECK(!memcmp(g_nativeHandsCache[0].original+offset,nativeUpdated,22*64));
        memcpy(once,palette,sizeof(once));
        for(unsigned n=0;n<100;++n){CHECK(Apply());CHECK(!memcmp(once,palette,sizeof(once)));}
    }
    // A controller at its shoulder still reaches its exact pose, with a rigid
    // folded arm; it must not leave the stock wrist behind or affect its peer.
    Reset();g_hands[1].grip.position={.15f,0,0};CHECK(Apply());
    CHECK(Near(palette[27][12],.15f));CHECK(Near(palette[5][12],-.5f));
    CHECK(std::fabs(d2vr_hands::Length(d2vr_hands::Position(palette[26])-d2vr_hands::Position(palette[25]))-.3f)<.001f);
    CHECK(std::fabs(d2vr_hands::Length(d2vr_hands::Position(palette[27])-d2vr_hands::Position(palette[26]))-.3f)<.001f);
    Untouched(0,2);Untouched(46,130);
    Reject([]{g_hands[0].gripValid=g_hands[1].gripValid=false;});
    for(bool alternate:{false,true}) {
        Reset();g_nativeGameplay.grab=d2vr_input::GrabFeedback::Busy;g_nativeGameplay.alternateHeldTarget=alternate;
        CHECK(Apply());CHECK(Near(palette[5][12],-.5f) && Near(palette[27][12],.5f));
        Untouched(0,2);Untouched(46,130); // root and demon attachment skeleton untouched
        float onceHeld[130][16];memcpy(onceHeld,palette,sizeof(onceHeld));
        for(unsigned n=0;n<100;++n){CHECK(Apply());CHECK(!memcmp(onceHeld,palette,sizeof(onceHeld)));}
    }
    for(unsigned hand=0;hand<2;++hand) {
        Reset();equippedAim[hand]=true;weaponAim[hand]={{0,0,0,1},{hand?.6f:-.6f,.3f,-.15f}};
        CHECK(Apply());const unsigned socket=hand?43:21;
        CHECK(Near(palette[socket][12],hand?.6f:-.6f));
        CHECK(Near(palette[socket][13],.3f) && Near(palette[socket][14],.15f));
        CHECK(Near(palette[hand?5:27][12],hand?-.5f:.5f));
    }
    Reject([]{g_nativeGameplay.grab=d2vr_input::GrabFeedback::Unknown;});
    Reject([]{g_protectedTrackedHands=false;});Reject([]{g_haveViews=false;});Reject([]{g_in3DGameplayMode=false;});
    Reject([]{g_trackedFrame.open=false;});Reject([]{g_trackedFrame.flags=0;});Reject([]{g_renderThreadId++;});
    Reject([]{g_protectedRightTentacle.paused=true;});Reject([]{g_protectedLeftTentacle.paused=true;});
    Reject([]{g_headGeometryReferenceValid=false;});Reject([]{g_nativeGameplay.time=800;});
    Reject([]{names[5].name="wrong";});Reject([]{Word(renderer,0x424,3);});Reject([]{Word(renderer,0x448,2);});
    Reject([]{Word(actor,0,0);});Reject([]{Ref(renderer,0x5BC,nullptr);});
    Reset();PublishProtectedHandPoses();NativeWeaponPose poses[2];CHECK(CopyProtectedHandPoses(Ptr(camera),poses));
    CHECK(!CopyProtectedHandPoses(Ptr(camera)+4,poses));clockMs+=100;
    CHECK(!ApplyNativeHandsChecked(renderer,0xFFC7C0,resource,true));
    ResetProtectedHandPoses();CHECK(!CopyProtectedHandPoses(Ptr(camera),poses));
    // Invoke the actual private-table wrapper through its native thiscall ABI.
    Reset();PublishProtectedHandPoses();g_protectedHandsPackOriginal=reinterpret_cast<ProtectedHandsPackMethod>(&Original);
    DWORD argument[3]={Ptr(renderer),1,0};expectedMxcsr=_mm_getcsr();__asm fnstsw expectedStatus
    SetLastError(0x12345678);
    reinterpret_cast<ProtectedHandsPackMethod>(g_protectedHandsTable.Table()[0xEC/4])(resource,argument);
    CHECK(originalCalls==1 && argument[1]==0 && GetLastError()==0x87654321 && g_nativeHandsLastWrite.sequence);
    CHECK(!g_insideProtectedHandsPack);
    Reset();g_protectedTrackedHands=false;argument[1]=1;SetLastError(0x12345678);
    ProtectedHandsPackHook(resource,nullptr,argument);
    CHECK(originalCalls==1 && argument[1]==0 && !g_nativeHandsWriteSeq && GetLastError()==0x87654321);
    Reset();g_nativeHandsSelectedResource=nullptr;argument[1]=1;SetLastError(0x12345678);
    ProtectedHandsPackHook(resource,nullptr,argument);
    CHECK(originalCalls==1 && argument[1]==0 && !g_nativeHandsWriteSeq);
    Reset();PublishProtectedHandPoses();recursive=true;argument[1]=1;SetLastError(0x12345678);
    ProtectedHandsPackHook(resource,nullptr,argument);CHECK(originalCalls==2 && argument[1]==0 && !g_insideProtectedHandsPack);
    CHECK(g_nativeHandsWriteSeq==1);
    CHECK(sourceTable[0xEC/4]==reinterpret_cast<void*>(&Original));
    // A cinematic retires our cached IK through the real pack hook. Preserve
    // native animation that independently replaced one arm after our last edit.
    Reset();CHECK(Apply());g_nativeCinematicPlaying=true;argument[1]=1;SetLastError(0x12345678);
    ProtectedHandsPackHook(resource,nullptr,argument);
    CHECK(!memcmp(palette,pristine,sizeof(pristine)) && originalCalls==1 && argument[1]==0);
    CHECK(!g_nativeHandsCache[0].appliedValid[0] && !g_nativeHandsCache[0].appliedValid[1]);
    CHECK(renderer[0x580] && renderer[0x581]);
    Reset();CHECK(Apply());palette[5][12]=123.f;g_nativeCinematicPlaying=true;argument[1]=1;SetLastError(0x12345678);
    ProtectedHandsPackHook(resource,nullptr,argument);
    CHECK(palette[5][12]==123.f && !memcmp(palette+24,pristine+24,22*64));
    // Gameplay resumes with a fresh solution, never the retired arm palette.
    g_nativeCinematicPlaying=false;CHECK(Apply());CHECK(Near(palette[27][12],.5f));
    Reset();g_nativeCinematicPlaying=true;CHECK(NativeCinematicPlaying());
    ++g_trackedFrame.id;CHECK(!NativeCinematicPlaying());--g_trackedFrame.id;
    g_trackedFrame.open=false;CHECK(!NativeCinematicPlaying());g_trackedFrame.open=true;
    ++g_renderThreadId;CHECK(!NativeCinematicPlaying());--g_renderThreadId;
    g_nativeCinematicAdmitted=false;CHECK(!NativeCinematicPlaying());
    for(unsigned i=0;i<96;++i)if(i!=0xEC/4)CHECK(g_protectedHandsTable.Table()[i]==sourceTable[i]);
    if(argc==2)NativePackOracle(argv[1]);
    printf("PASS actual protected hands: %u checks; per-hand tracking/IK failure, no pistol/support gate, pristine repeated pack, ownership/hold refusals, native ABI/ref once, floating state, LastError, reentrancy\n",checks);
    HeapFree(GetProcessHeap(),0,resource);
    return 0;
}
