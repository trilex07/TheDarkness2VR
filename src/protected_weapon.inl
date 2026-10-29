#ifdef D2VR_MATCHED_CAPTURE
#include "native_weapon_bridge.h"
#include "psvr2/ShotEvents.h"
#include "native_weapon_prefix_plan.h"
static d2vr_native_view::InstanceTable g_protectedWeaponModelTable,g_protectedWeaponFireTable;
static d2vr_native_clone::Code g_protectedWeaponModelCode,g_protectedWeaponFireCode;
static bool g_protectedWeaponReady=false,g_protectedWeaponPrepareAttempted=false;
static bool ProtectedWeaponModelOwned(void* model) {
    return g_protectedWeaponReady && g_protectedWeaponModelTable.Owns(model);
}
static bool ProtectedWeaponFireOwned(void* fire) {
    return g_protectedWeaponReady && g_protectedWeaponFireTable.Owns(fire);
}
static bool ProtectedWeaponAvailable() {
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    return g_protectedTrackedHands && g_protectedWeaponReady && g_nativeImageGuard.enabled &&
        g_nativePlayerOwnerAdmitted && g_imagePreservingInput && g_protectedGestureActions &&
        g_haveViews && g_in3DGameplayMode && g_trackedFrame.open && g_trackedFrame.id &&
        GetCurrentThreadId()==g_renderThreadId && (g_trackedFrame.flags&tracked)==tracked &&
        !g_protectedRightTentacle.paused && !g_protectedLeftTentacle.paused &&
        g_headGeometry && g_headGeometryReferenceValid;
}
static bool ProtectedGunType(void* weapon) {
    if(!weapon)return false;
    __try {
        if(*static_cast<DWORD*>(weapon)==0x1010E30)return true;
        // CFBBA0 itself admits Gun descendants with native IsA(1119658).
        // 4BF720 walks instance+4 -> descriptor+0C; concrete vptr equality
        // unnecessarily excludes derived guns with the same base layout.
        DWORD type=*reinterpret_cast<DWORD*>(static_cast<BYTE*>(weapon)+4);
        for(unsigned depth=0;type && depth<32;++depth) {
            if(type==0x1119658)return true;
            type=*reinterpret_cast<DWORD*>(type+0xC);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return false;
}
// Called only while a native receiver is live on this render thread. Rewalk
// the owner's equipped slot each time; a cached model identity cannot admit a
// dropped/replaced gun or a newly held enemy. Nothing retains instance pointers.
static bool ProtectedWeaponOwner(void* camera,unsigned slot,void*& gun,void*& model,void*& fire) {
    if(slot>=2 || !camera || !ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return false;
    __try {
        auto avatar=**reinterpret_cast<BYTE***>(static_cast<BYTE*>(camera)+0x30);
        auto inventory=**reinterpret_cast<BYTE***>(avatar+0x630);
        auto weapon=**reinterpret_cast<BYTE***>(inventory+0xB0+12*slot);
        if(!ProtectedGunType(weapon) ||
           **reinterpret_cast<BYTE***>(weapon+0x980)!=avatar)return false;
        const DWORD mode=*reinterpret_cast<DWORD*>(weapon+0xA78);
        if(mode>=4)return false;
        auto parts=*reinterpret_cast<BYTE**>(weapon+0xA64);
        auto action=**reinterpret_cast<BYTE***>(parts+48*mode+0xC);
        if(!action || (*reinterpret_cast<DWORD*>(action)!=0x1010AE0 && !ProtectedWeaponFireOwned(action)) ||
           *reinterpret_cast<DWORD*>(action+4)!=0x11193F0 ||
           **reinterpret_cast<BYTE***>(action+0x4C)!=weapon)return false;
        // The shot action owns its equipped gun independently of the optional
        // +12C model reference. The live secondary pistol can have an empty
        // reference here. Never let that strand its proven fire action on the
        // common camera ray, and never admit an unknown model to fix a shot.
        gun=weapon;fire=action;model=nullptr;
        auto entity=**reinterpret_cast<BYTE***>(weapon+0x12C);
        if(entity && (*reinterpret_cast<DWORD*>(entity)==0xFD6BC0 || ProtectedWeaponModelOwned(entity)))model=entity;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool ResolveProtectedWeapon(void* model,void* fire,void* weaponRef,float* world,unsigned* stage=nullptr) {
    if(stage)*stage=1; // frame/tracking/thread context
    if(!world || (!model && !fire) || !ProtectedWeaponAvailable())return false;
    if(stage)*stage=2; // fresh selected-player sample
    NativeGameplaySample owner;
    if(!CopyNativeGameplay(owner))return false;
    void* camera=reinterpret_cast<void*>(owner.camera);
    // A demon hold does not transfer ownership of the separately equipped gun.
    // Revalidate that gun's live inventory/model/fire chain below; never steer
    // the demon's held entity or its throw/attachment/physics branch here.
    __try {
        if(stage)*stage=3; // reciprocal camera owner
        if(!ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return false;
        if(stage)*stage=4; // selected fire/model must actually be equipped
        for(unsigned slot=0;slot<2;++slot) {
            void *gun=nullptr,*equippedModel=nullptr,*equippedFire=nullptr;
            if(!ProtectedWeaponOwner(camera,slot,gun,equippedModel,equippedFire) ||
               (model && model!=equippedModel) || (fire && fire!=equippedFire))continue;
            if(stage)*stage=5+slot*16; // instance publication
            if(!ProtectedWeaponFireOwned(equippedFire) ||
               (model && !ProtectedWeaponModelOwned(equippedModel)))return false;
            if(stage)*stage=6+slot*16; // native shot argument's gun reference
            if(fire && (!weaponRef || **static_cast<void***>(weaponRef)!=gun || !ProtectedWeaponFireOwned(fire)))return false;
            if(model && !ProtectedWeaponModelOwned(model))return false;
            const unsigned hand=1-slot;
            if(stage)*stage=7+slot*16; // active controller tracking/scale
            if(!g_hands[hand].aimValid || !std::isfinite(g_gameUnitsPerMeter) ||
               g_gameUnitsPerMeter<.01f || g_gameUnitsPerMeter>5000.f)return false;
            const auto& ref=g_headGeometryReference;const auto& aim=g_hands[hand].aim;
            const float rq[4]={ref.orientation.x,ref.orientation.y,ref.orientation.z,ref.orientation.w};
            const float rp[3]={ref.position.x,ref.position.y,ref.position.z};
            const float aq[4]={aim.orientation.x,aim.orientation.y,aim.orientation.z,aim.orientation.w};
            const float ap[3]={aim.position.x,aim.position.y,aim.position.z};
            float reference[16],inverse[16],tracking[16],local[16],result[16];
            if(stage)*stage=8+slot*16; // tracking transforms
            if(!d2vr_math::TrackingPose(rq,rp,reference) || !d2vr_math::Invert(reference,inverse) ||
               !d2vr_math::TrackingPose(aq,ap,tracking))return false;
            d2vr_math::Multiply(tracking,inverse,local);
            const auto base=reinterpret_cast<const float*>(static_cast<BYTE*>(camera)+0x80);
            if(stage)*stage=9+slot*16; // native camera matrix contract
            if(!d2vr_aim::BuildScaledWeaponMatrix(base,local,g_gameUnitsPerMeter,result))return false;
            if(stage)*stage=32+slot; // final consumer receives right/left ray
            std::memcpy(world,result,sizeof(result));return true;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){if(stage)*stage=10;return false;}
    return false;
}
static bool CopyProtectedWeaponHandPose(DWORD camera,unsigned hand,XrPosef& pose) {
    if(hand>=2 || !ProtectedWeaponAvailable() || !g_hands[hand].aimValid)return false;
    void *gun=nullptr,*model=nullptr,*fire=nullptr;
    // A live secondary pistol can have no separate +12C model. Its selected
    // human weapon socket still needs the same aim space as the owned shot;
    // falling back to grip rotates the visible gun away from its actual ray.
    // This selects a pose for our already-owned human palette only. It neither
    // admits nor writes the optional (possibly unknown) model instance.
    if(!ProtectedWeaponOwner(reinterpret_cast<void*>(camera),1-hand,gun,model,fire) ||
       !ProtectedWeaponFireOwned(fire))return false;
    pose=g_hands[hand].aim;return true;
}
static bool ResolveProtectedWeaponModel(const float* current,float* candidate) {
    if(!current || !candidate)return false;
    float world[16];
    if(!ResolveProtectedWeapon(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(current)-0x140),nullptr,nullptr,world))return false;
    __try {std::memcpy(candidate,world,sizeof(world));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool ResolveProtectedWeaponFire(void* fire,void* weaponRef,float* world) {
    unsigned stage=0;
    const bool applied=ResolveProtectedWeapon(nullptr,fire,weaponRef,world,&stage);
    // Publication alone is not proof that the firing virtual is consumed or
    // that its context admits a controller ray. Bound receipts to two per
    // outcome on this thread; no per-frame tracing or retained native pointers.
    static thread_local BYTE reports[34]{};
    if(stage<_countof(reports) && reports[stage]<2) {
        ++reports[stage];
        Log("[ProtectedWeaponShot] stage=%u applied=%d fire=%p ref=%p frame=%llu open=%d renderThread=%d gameplay=%d flags=%llu aimL=%d aimR=%d pauseL=%d pauseR=%d; stage32=right 33=left 1=context 2=sample 3=camera 4=equipped 5/21=publication 6/22=gunRef 7/23=tracking 8/24=pose 9/25=base 10=exception",
            stage,int(applied),fire,weaponRef,g_trackedFrame.id,int(g_trackedFrame.open),
            int(GetCurrentThreadId()==g_renderThreadId),int(g_in3DGameplayMode),
            static_cast<unsigned long long>(g_trackedFrame.flags),int(g_hands[0].aimValid),int(g_hands[1].aimValid),
            int(g_protectedLeftTentacle.paused),int(g_protectedRightTentacle.paused));
    }
    return applied;
}
static void ReportProtectedWeaponShot(void* fire,void* weaponRef,bool fired) {
    if(!fired || !ProtectedWeaponAvailable())return;
    NativeGameplaySample owner;
    if(!CopyNativeGameplay(owner))return;
    __try {
        for(unsigned slot=0;slot<2;++slot) {
            void *gun=nullptr,*model=nullptr,*equippedFire=nullptr;
            if(ProtectedWeaponOwner(reinterpret_cast<void*>(owner.camera),slot,gun,model,equippedFire) &&
               equippedFire==fire && ProtectedWeaponFireOwned(fire) && weaponRef &&
               **static_cast<void***>(weaponRef)==gun) {
                d2vr::psvr2::noteShot(1-slot);return;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
static bool PrepareProtectedWeaponCode() {
    if(g_protectedWeaponReady)return true;
    if(g_protectedWeaponPrepareAttempted)return false;
    g_protectedWeaponPrepareAttempted=true;
    using namespace d2vr_native_clone;
    // Full model/fire functions, every copied-prefix helper, and complete
    // class tables/header must match before any live-instance publication.
    if(!MatchesDigest(reinterpret_cast<void*>(0xFD6BBC),(weaponModelTableCount+1)*4,weaponModelTableDigest) ||
       !MatchesDigest(reinterpret_cast<void*>(0x1010ADC),(weaponFireTableCount+1)*4,weaponFireTableDigest) ||
       !MatchesDigest(weaponFireBlock.source,weaponFireBlock.bytes,weaponFireBlock.sha256) ||
       !MatchesDigest(weaponShotConsumer.source,weaponShotConsumer.bytes,weaponShotConsumer.sha256))return false;
    for(const auto& block:weaponHelpers)if(!MatchesDigest(block.source,block.bytes,block.sha256))return false;
    HMODULE self=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(d2vr_weapon_bridge::Bridge::ModelEntry()),&self) ||
       !d2vr_weapon_bridge::Bridge::Prepare(reinterpret_cast<d2vr_weapon_bridge::Compare>(0x481F50),
           reinterpret_cast<d2vr_weapon_bridge::Fire>(0xC47570),ResolveProtectedWeaponModel,
           ResolveProtectedWeaponFire,g_nativeImageGuard,ReportProtectedWeaponShot))return false;
    const Override comparison{0,0x262,d2vr_weapon_bridge::Bridge::ModelEntry()};
    // CFBBA0 can replace its incoming matrix using the native camera. Change
    // only its final consumer call, after that computation. Its retained refs,
    // camera queries, no-shot branch and cleanup still execute in native order.
    const Override shot{0,0xCFBD60-0xCFBBA0,d2vr_weapon_bridge::Bridge::FireEntry()};
    if(!g_protectedWeaponModelCode.Prepare(&weaponModelBlock,1,weaponModelCalls,
        _countof(weaponModelCalls),&comparison,g_nativeImageGuard,nullptr,&weaponModelPrefix) ||
       !g_protectedWeaponModelTable.Prepare(reinterpret_cast<void**>(0xFD6BC0),weaponModelTableCount,
           0xF0/4,g_protectedWeaponModelCode.Entry(0),g_nativeImageGuard) ||
       !g_protectedWeaponFireCode.Prepare(&weaponFireBlock,1,weaponFireCalls,
           _countof(weaponFireCalls),&shot,g_nativeImageGuard) ||
       !g_protectedWeaponFireTable.Prepare(reinterpret_cast<void**>(0x1010AE0),weaponFireTableCount,
           0x64/4,g_protectedWeaponFireCode.Entry(0),g_nativeImageGuard))return false;
    g_protectedWeaponReady=true;return true;
}
static void ReportProtectedWeaponRefusal(void* camera,unsigned slot) {
    // One receipt per changed equipped identity/type, not a per-frame trace.
    // This distinguishes an unsupported gun/model/fire variant from bad aim.
    static DWORD previous[2][6]{};
    DWORD state[6]{};
    __try {
        auto avatar=**reinterpret_cast<BYTE***>(static_cast<BYTE*>(camera)+0x30);
        auto inventory=**reinterpret_cast<BYTE***>(avatar+0x630);
        auto gun=**reinterpret_cast<BYTE***>(inventory+0xB0+12*slot);
        if(!gun)return;
        state[0]=DWORD(reinterpret_cast<uintptr_t>(gun));
        state[1]=*reinterpret_cast<DWORD*>(gun);state[2]=*reinterpret_cast<DWORD*>(gun+4);
        if(ProtectedGunType(gun)) {
            auto model=**reinterpret_cast<BYTE***>(gun+0x12C);
            state[3]=model?*reinterpret_cast<DWORD*>(model):0;
            const DWORD mode=*reinterpret_cast<DWORD*>(gun+0xA78);
            if(mode<4) {
                auto parts=*reinterpret_cast<BYTE**>(gun+0xA64);
                auto fire=**reinterpret_cast<BYTE***>(parts+48*mode+0xC);
                state[4]=fire?*reinterpret_cast<DWORD*>(fire):0;
                state[5]=fire?*reinterpret_cast<DWORD*>(fire+4):0;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){return;}
    if(std::memcmp(previous[slot],state,sizeof(state))) {
        std::memcpy(previous[slot],state,sizeof(state));
        Log("[ProtectedWeapon] REFUSED slot=%u gun=%08lX gunTable=%08lX type=%08lX modelTable=%08lX fireTable=%08lX fireType=%08lX; native retained",slot,state[0],state[1],state[2],state[3],state[4],state[5]);
    }
}
static void PrepareProtectedWeaponForCamera(void* camera) {
    if(!g_protectedTrackedHands || !g_nativeImageGuard.enabled || !g_nativePlayerOwnerAdmitted ||
       GetCurrentThreadId()!=g_renderThreadId || !ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return;
    for(unsigned slot=0;slot<2;++slot) {
        void *gun=nullptr,*model=nullptr,*fire=nullptr;
        if(!ProtectedWeaponOwner(camera,slot,gun,model,fire)) {
            ReportProtectedWeaponRefusal(camera,slot);continue;
        }
        const bool needsShot=!ProtectedWeaponFireOwned(fire);
        const bool needsPose=model && !ProtectedWeaponModelOwned(model);
        if(!needsShot && !needsPose)continue;
        if(!PrepareProtectedWeaponCode()) {
            static bool reported=false;
            if(!reported) {
                reported=true;
                Log("[ProtectedWeapon] native contract/code preparation refused (%s); native model/fire retained",g_protectedWeaponModelCode.Status());
            }
            return;
        }
        const bool shot=!needsShot || g_protectedWeaponFireTable.Publish(fire,g_nativeImageGuard);
        const bool pose=shot && model && (!needsPose || g_protectedWeaponModelTable.Publish(model,g_nativeImageGuard));
        if(shot || pose)Log("[ProtectedWeapon] slot=%u model=%p fire=%p privatePose=%d privateShot=%d; independent aim spaces, original AD94A7 updates; native CFBBA0 flow with final C47570 controller ray, HMD validation pending",slot,model,fire,int(pose),int(shot));
    }
}
#else
static bool ProtectedWeaponFireOwned(void*) {return false;}
static bool ProtectedWeaponModelOwned(void*) {return false;}
static bool CopyProtectedWeaponHandPose(DWORD,unsigned,XrPosef&) {return false;}
static void PrepareProtectedWeaponForCamera(void*) {}
#endif
