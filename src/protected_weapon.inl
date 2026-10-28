#ifdef D2VR_MATCHED_CAPTURE
#include "native_weapon_bridge.h"
#include "native_weapon_prefix_plan.h"
static d2vr_native_view::InstanceTable g_protectedWeaponModelTable,g_protectedWeaponFireTable;
static d2vr_native_clone::Code g_protectedWeaponModelCode;
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
// Called only while a native receiver is live on this render thread. Rewalk
// the owner's equipped slot each time; a cached model identity cannot admit a
// dropped/replaced gun or a newly held enemy. Nothing retains instance pointers.
static bool ProtectedWeaponOwner(void* camera,unsigned slot,void*& gun,void*& model,void*& fire) {
    if(slot>=2 || !camera || !ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return false;
    __try {
        auto avatar=**reinterpret_cast<BYTE***>(static_cast<BYTE*>(camera)+0x30);
        auto inventory=**reinterpret_cast<BYTE***>(avatar+0x630);
        auto weapon=**reinterpret_cast<BYTE***>(inventory+0xB0+12*slot);
        if(!weapon || *reinterpret_cast<DWORD*>(weapon)!=0x1010E30 ||
           **reinterpret_cast<BYTE***>(weapon+0x980)!=avatar)return false;
        auto entity=**reinterpret_cast<BYTE***>(weapon+0x12C);
        if(!entity || (*reinterpret_cast<DWORD*>(entity)!=0xFD6BC0 && !ProtectedWeaponModelOwned(entity)))return false;
        const DWORD mode=*reinterpret_cast<DWORD*>(weapon+0xA78);
        if(mode>=4)return false;
        auto parts=*reinterpret_cast<BYTE**>(weapon+0xA64);
        auto action=**reinterpret_cast<BYTE***>(parts+48*mode+0xC);
        if(!action || (*reinterpret_cast<DWORD*>(action)!=0x1010AE0 && !ProtectedWeaponFireOwned(action)) ||
           *reinterpret_cast<DWORD*>(action+4)!=0x11193F0 ||
           **reinterpret_cast<BYTE***>(action+0x4C)!=weapon)return false;
        gun=weapon;model=entity;fire=action;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool ResolveProtectedWeapon(void* model,void* fire,void* weaponRef,float* world) {
    if(!world || (!model && !fire) || !ProtectedWeaponAvailable())return false;
    NativeGameplaySample owner;
    if(!CopyNativeGameplay(owner))return false;
    void* camera=reinterpret_cast<void*>(owner.camera);
    // Refresh held-state fields at consumption, not just their prior sample.
    __try {
        if(!ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return false;
        auto avatar=**reinterpret_cast<BYTE***>(static_cast<BYTE*>(camera)+0x30);
        auto inventory=**reinterpret_cast<BYTE***>(avatar+0x630);
        auto tentacle=**reinterpret_cast<BYTE***>(inventory+0x20C+6*4);
        if(!tentacle || *reinterpret_cast<DWORD*>(tentacle)!=0xFD6E00 ||
           **reinterpret_cast<BYTE***>(tentacle+0x980)!=avatar)return false;
        const DWORD grabMode=*reinterpret_cast<DWORD*>(tentacle+0xA78);
        if(grabMode>=4)return false;
        auto grabParts=*reinterpret_cast<BYTE**>(tentacle+0xA64);
        auto grab=**reinterpret_cast<BYTE***>(grabParts+48*grabMode+0xC);
        if(!grab || (*reinterpret_cast<DWORD*>(grab)!=0x1004FE8 && !ProtectedThrowFireOwned(grab)) ||
           *reinterpret_cast<DWORD*>(grab+4)!=0x11126F0 || **reinterpret_cast<BYTE***>(grab+0x4C)!=tentacle)return false;
        const auto alternate=*reinterpret_cast<DWORD**>(grab+0x584);
        if(**reinterpret_cast<DWORD**>(grab+0x558) || **reinterpret_cast<DWORD**>(grab+0x580) ||
           (alternate && *alternate) || **reinterpret_cast<DWORD**>(grab+0x590))return false;
        for(unsigned slot=0;slot<2;++slot) {
            void *gun=nullptr,*equippedModel=nullptr,*equippedFire=nullptr;
            if(!ProtectedWeaponOwner(camera,slot,gun,equippedModel,equippedFire) ||
               (model && model!=equippedModel) || (fire && fire!=equippedFire))continue;
            if(!ProtectedWeaponModelOwned(equippedModel) || !ProtectedWeaponFireOwned(equippedFire))return false;
            if(fire && (!weaponRef || **static_cast<void***>(weaponRef)!=gun || !ProtectedWeaponFireOwned(fire)))return false;
            if(model && !ProtectedWeaponModelOwned(model))return false;
            const unsigned hand=1-slot;
            if(!g_hands[hand].aimValid || !std::isfinite(g_gameUnitsPerMeter) ||
               g_gameUnitsPerMeter<.01f || g_gameUnitsPerMeter>5000.f)return false;
            const auto& ref=g_headGeometryReference;const auto& aim=g_hands[hand].aim;
            const float rq[4]={ref.orientation.x,ref.orientation.y,ref.orientation.z,ref.orientation.w};
            const float rp[3]={ref.position.x,ref.position.y,ref.position.z};
            const float aq[4]={aim.orientation.x,aim.orientation.y,aim.orientation.z,aim.orientation.w};
            const float ap[3]={aim.position.x,aim.position.y,aim.position.z};
            float reference[16],inverse[16],tracking[16],local[16],result[16];
            if(!d2vr_math::TrackingPose(rq,rp,reference) || !d2vr_math::Invert(reference,inverse) ||
               !d2vr_math::TrackingPose(aq,ap,tracking))return false;
            d2vr_math::Multiply(tracking,inverse,local);
            const auto base=reinterpret_cast<const float*>(static_cast<BYTE*>(camera)+0x80);
            if(!d2vr_aim::BuildScaledWeaponMatrix(base,local,g_gameUnitsPerMeter,result))return false;
            std::memcpy(world,result,sizeof(result));return true;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return false;
}
static bool ResolveProtectedWeaponModel(const float* current,float* candidate) {
    if(!current || !candidate)return false;
    float world[16];
    if(!ResolveProtectedWeapon(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(current)-0x140),nullptr,nullptr,world))return false;
    __try {std::memcpy(candidate,world,sizeof(world));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool ResolveProtectedWeaponFire(void* fire,void* weaponRef,float* world) {
    return ResolveProtectedWeapon(nullptr,fire,weaponRef,world);
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
       !MatchesDigest(weaponFireBlock.source,weaponFireBlock.bytes,weaponFireBlock.sha256))return false;
    for(const auto& block:weaponHelpers)if(!MatchesDigest(block.source,block.bytes,block.sha256))return false;
    HMODULE self=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(d2vr_weapon_bridge::Bridge::ModelEntry()),&self) ||
       !d2vr_weapon_bridge::Bridge::Prepare(reinterpret_cast<d2vr_weapon_bridge::Compare>(0x481F50),
           reinterpret_cast<d2vr_weapon_bridge::Fire>(0xCFBBA0),ResolveProtectedWeaponModel,
           ResolveProtectedWeaponFire,g_nativeImageGuard))return false;
    const Override comparison{0,0x262,d2vr_weapon_bridge::Bridge::ModelEntry()};
    if(!g_protectedWeaponModelCode.Prepare(&weaponModelBlock,1,weaponModelCalls,
        _countof(weaponModelCalls),&comparison,g_nativeImageGuard,nullptr,&weaponModelPrefix) ||
       !g_protectedWeaponModelTable.Prepare(reinterpret_cast<void**>(0xFD6BC0),weaponModelTableCount,
           0xF0/4,g_protectedWeaponModelCode.Entry(0),g_nativeImageGuard) ||
       !g_protectedWeaponFireTable.Prepare(reinterpret_cast<void**>(0x1010AE0),weaponFireTableCount,
           0x64/4,d2vr_weapon_bridge::Bridge::FireEntry(),g_nativeImageGuard))return false;
    g_protectedWeaponReady=true;return true;
}
static void PrepareProtectedWeaponForCamera(void* camera) {
    if(!g_protectedTrackedHands || !g_nativeImageGuard.enabled || !g_nativePlayerOwnerAdmitted ||
       GetCurrentThreadId()!=g_renderThreadId || !ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return;
    for(unsigned slot=0;slot<2;++slot) {
        void *gun=nullptr,*model=nullptr,*fire=nullptr;
        if(!ProtectedWeaponOwner(camera,slot,gun,model,fire) ||
           (ProtectedWeaponModelOwned(model) && ProtectedWeaponFireOwned(fire)))continue;
        if(!PrepareProtectedWeaponCode()) {
            static bool reported=false;
            if(!reported) {
                reported=true;
                Log("[ProtectedWeapon] native contract/code preparation refused (%s); native model/fire retained",g_protectedWeaponModelCode.Status());
            }
            return;
        }
        const bool shot=g_protectedWeaponFireTable.Publish(fire,g_nativeImageGuard);
        const bool pose=shot && g_protectedWeaponModelTable.Publish(model,g_nativeImageGuard);
        if(shot || pose)Log("[ProtectedWeapon] slot=%u model=%p fire=%p privatePose=%d privateShot=%d; independent aim spaces, original AD94A7 updates and CFBBA0 fire, HMD validation pending",slot,model,fire,int(pose),int(shot));
    }
}
#else
static bool ProtectedWeaponModelOwned(void*) {return false;}
static void PrepareProtectedWeaponForCamera(void*) {}
#endif
