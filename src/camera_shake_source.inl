// camera_shake_source.inl - remove first-person camera shake at its native source (heap only).
//
// Static evidence (EXE D46BB6CB...4EC1, docs/research/darkness2-camera-shake-20261101.md):
//  * class 00FDF170 owns procedural blocks +8C0 (mViewShake), +910 (mRumbleShake),
//    +960 (mZoomShake), +CA0; generator 00628420 scales outputs by mShakeFactorRot
//    (+10..+18) and mShakeFactorPos (+24), filled once by loader B2EEE0.
//  * +1170 (mImpactShake) holds four sub-systems at +0,+70 (springs 9304A0) and +E0,+130
//    (noise 5D1760). Update, impulse (762DA0/C778C0) and output getter A2FD90 all test the
//    byte +10: zero = skipped, getter returns a static zero vector.
// The live owner is not the one reached from the selected avatar (+5E0 owner stayed idle in
// the headset log), so every 00FDF170 instance in private memory is found by a background
// scan and suppressed. Factors are zeroed; amplitudes keep evolving natively and are read
// back as the rumble signal (hits/blasts add impulses to +20 of the procedural blocks).
struct CameraShakeSourceState {
    SRWLOCK lock=SRWLOCK_INIT;
    DWORD owners[16]{};unsigned count=0;
    volatile LONG scanning=0;ULONGLONG nextScan=0,start=0,nextLog=0;
    std::atomic<unsigned long long> scans{0};
    unsigned long long writes=0,refills=0;
    float impulse=0.f,peakImpulse=0.f;
    DWORD activeOwner=0;
};
#include "camera_event_feedback.h"
static CameraShakeSourceState g_cameraShakeSource;
static volatile LONG g_cameraShakeImpulseMilli=0,g_cameraShakeImpulseTick=0;
static volatile LONG g_cameraEventRumbleMilli=0;
static std::atomic<ULONGLONG> g_cameraEventRumbleTick{0};

static DWORD WINAPI CameraShakeSourceScan(void*) {
    DWORD found[16]{};unsigned count=0;
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    uintptr_t address=0x10000;
    MEMORY_BASIC_INFORMATION mbi{};
    while(address<0x7FFF0000 && VirtualQuery(reinterpret_cast<void*>(address),&mbi,sizeof(mbi))==sizeof(mbi)) {
        const uintptr_t base=reinterpret_cast<uintptr_t>(mbi.BaseAddress);const uintptr_t end=base+uintptr_t(mbi.RegionSize);
        if(end<=address)break;
        if(mbi.State==MEM_COMMIT && mbi.Type==MEM_PRIVATE && (mbi.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE)) &&
           !(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && !(base<=image && image<end)) {
            __try {
                for(auto p=reinterpret_cast<const DWORD*>(base);reinterpret_cast<uintptr_t>(p)+0x1300<=end && count<16;++p)
                    if(IsCameraSourceTable(*p)) {
                        const auto o=reinterpret_cast<const BYTE*>(p);
                        bool ok=true;
                        for(unsigned b: {0x8C0u,0x910u,0x960u,0xCA0u})
                            for(unsigned f: {0x10u,0x14u,0x18u,0x24u}) {
                                const float v=*reinterpret_cast<const float*>(o+b+f);
                                if(!std::isfinite(v) || std::fabs(v)>100.f)ok=false;
                            }
                        if(ok)found[count++]=DWORD(reinterpret_cast<uintptr_t>(p));
                    }
            } __except(EXCEPTION_EXECUTE_HANDLER){}
        }
        address=end;
    }
    auto& st=g_cameraShakeSource;
    AcquireSRWLockExclusive(&st.lock);
    std::memcpy(st.owners,found,sizeof(found));st.count=count;++st.scans;
    ReleaseSRWLockExclusive(&st.lock);
    Log("[CameraShakeSource] scan=%llu owners=%u first=%08lX; heap objects only",st.scans.load(),count,count?found[0]:0);
    InterlockedExchange(&st.scanning,0);
    return 0;
}

static void RequestCameraShakeSourceScan() {
    auto& st=g_cameraShakeSource;
    const auto now=GetTickCount64();
    if(now>=st.nextScan && !InterlockedCompareExchange(&st.scanning,1,0)) {
        st.nextScan=now+(st.scans<3?5000:30000);
        if(HANDLE thread=CreateThread(nullptr,0,CameraShakeSourceScan,nullptr,0,nullptr))CloseHandle(thread);
        else InterlockedExchange(&st.scanning,0);
    }
}

// Selected-player observer. The memory search finds candidates; the verified
// back-reference admits ONE owner. Optional source hook publishes only on that owner.
static void ObserveSelectedCameraEvent(DWORD avatar) {
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_cameraEventRumble && !g_cameraNativeNoShake)return;
    DWORD owners[16]{};unsigned count=0;
    auto& st=g_cameraShakeSource;
    AcquireSRWLockShared(&st.lock);
    std::memcpy(owners,st.owners,sizeof(owners));count=st.count;
    ReleaseSRWLockShared(&st.lock);
    d2vr_camera_event::Sample sample{};DWORD selected=0;unsigned matched=0;
    for(unsigned i=0;i<count;++i) {
        if(!CameraSourceOwnerMatches(owners[i],avatar))continue;
        __try {
            const auto owner=reinterpret_cast<const BYTE*>(owners[i]);
            d2vr_camera_event::Sample candidate{};
            std::memcpy(candidate.impactPosition,owner+0x12F0,sizeof(candidate.impactPosition));
            std::memcpy(candidate.impactRotation,owner+0x1300,sizeof(candidate.impactRotation));
            std::memcpy(candidate.environmentalPosition,owner+0xCA0,sizeof(candidate.environmentalPosition));
            std::memcpy(candidate.environmentalRotation,owner+0xCD8,sizeof(candidate.environmentalRotation));
            if(CameraSourceOwnerMatches(owners[i],avatar)){sample=candidate;selected=owners[i];++matched;}
        } __except(EXCEPTION_EXECUTE_HANDLER){}
    }
    if(matched==1 && g_cameraNativeNoShake)PrepareCameraNativeNoShake(selected,avatar);
    const float amount=matched==1?d2vr_camera_event::Amount(sample):0.f;
    const auto now=GetTickCount64();
    // A verified live producer needs no periodic whole-process rescan. Refresh
    // only on initial discovery or after its avatar/type back-reference fails.
    if(matched!=1) {
        if(st.scans.load() && now+1000<st.nextScan)st.nextScan=now+1000;
        RequestCameraShakeSourceScan();
    }
    InterlockedExchange(&g_cameraEventRumbleMilli,LONG(amount*1000));
    g_cameraEventRumbleTick.store(now,std::memory_order_release);
    static DWORD lastOwner=0;static ULONGLONG nextLog=0;
    if(selected!=lastOwner || (amount>.05f && now>=nextLog)) {
        lastOwner=selected;nextLog=now+250;
        Log("[CameraEventRumble] owner=%08lX avatar=%08lX matches=%u amount=%.3f impact=(%.4f,%.4f,%.4f) environment=(%.4f,%.4f,%.4f); native outputs read only, delivery receipt is separate",
            selected,avatar,matched,amount,sample.impactPosition[0],sample.impactPosition[1],sample.impactPosition[2],
            sample.environmentalPosition[0],sample.environmentalPosition[1],sample.environmentalPosition[2]);
    }
#else
    (void)avatar;
#endif
}
static float SelectedCameraEventRumble() {
    if(!g_cameraEventRumble)return 0;
    const auto tick=g_cameraEventRumbleTick.load(std::memory_order_acquire);
    if(!tick || GetTickCount64()-tick>150)return 0;
    return float(InterlockedCompareExchange(&g_cameraEventRumbleMilli,0,0))*.001f;
}

// Render thread, selected-owner path, once per frame.
static void SuppressCameraShakeSources() {
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_cameraShakeSourceSuppress)return;
    auto& st=g_cameraShakeSource;
    const auto now=GetTickCount64();
    if(!st.start){st.start=now;Log("[CameraShakeSource] ACTIVE=1; zero procedural factors and impact enables on all 00FDF170 owners");}
    RequestCameraShakeSourceScan();
    DWORD owners[16];unsigned count;
    AcquireSRWLockShared(&st.lock);std::memcpy(owners,st.owners,sizeof(owners));count=st.count;ReleaseSRWLockShared(&st.lock);
    constexpr unsigned blocks[4]={0x8C0,0x910,0x960,0xCA0};
    constexpr unsigned factors[4]={0x10,0x14,0x18,0x24};
    constexpr unsigned impacts[4]={0x0,0x70,0xE0,0x130};
    float impulse=0.f,activity[16]{};bool stale=false;
    for(unsigned i=0;i<count;++i) {
        __try {
            auto o=reinterpret_cast<BYTE*>(owners[i]);
            if(*reinterpret_cast<const DWORD*>(o)!=0xFDF170){stale=true;continue;}
            for(unsigned b:blocks) {
                const float amp=*reinterpret_cast<const float*>(o+b+0x20);
                if(std::isfinite(amp) && b!=0xCA0)impulse=(std::max)(impulse,std::fabs(amp));
                for(unsigned f:factors) {
                    auto v=reinterpret_cast<float*>(o+b+f);
                    if(*v!=0.f){if(st.writes)++st.refills;*v=0.f;}
                }
                activity[i]+=std::fabs(*reinterpret_cast<const float*>(o+b+0x20))+std::fabs(*reinterpret_cast<const float*>(o+b+0x2C))+
                    std::fabs(*reinterpret_cast<const float*>(o+b+0x30));
            }
            for(unsigned k:impacts) {
                auto enable=o+0x1170+k+0x10;
                if(*enable){*enable=0;}
            }
            activity[i]+=std::fabs(*reinterpret_cast<const float*>(o+0x1F0))+std::fabs(*reinterpret_cast<const float*>(o+0x20C));
        } __except(EXCEPTION_EXECUTE_HANDLER){stale=true;}
    }
    if(count)++st.writes;
    if(stale && now+1000<st.nextScan)st.nextScan=now+1000; // freed/reused owner: rescan soon
    // Rumble from native impulses: rises immediately, decays over ~0.3 s.
    const float target=std::clamp((impulse-0.02f)/0.25f,0.f,1.f);
    st.impulse=target>st.impulse?target:st.impulse*0.92f;
    st.peakImpulse=(std::max)(st.peakImpulse,impulse);
    InterlockedExchange(&g_cameraShakeImpulseMilli,LONG(st.impulse*1000.f));
    InterlockedExchange(&g_cameraShakeImpulseTick,LONG(now&0x7fffffff));
    if(now>=st.nextLog && now-st.start<300000) {
        st.nextLog=now+500;
        unsigned best=0;for(unsigned i=1;i<count;++i)if(activity[i]>activity[best])best=i;
        Log("[CameraShakeSource] ms=%llu owners=%u writes=%llu refills=%llu peakImpulse=%.4f rumble=%.2f mostActive=%08lX activity=%.4f",
            static_cast<unsigned long long>(now-st.start),count,st.writes,st.refills,st.peakImpulse,st.impulse,
            count?owners[best]:0,count?activity[best]:0.f);
        st.peakImpulse=0.f;
    }
#endif
}
static float CameraShakeImpulseRumble() {
    if(!g_cameraShakeSourceSuppress)return 0.f;
    if(LONG(GetTickCount64()&0x7fffffff)-LONG(g_cameraShakeImpulseTick)>150)return 0.f;
    return float(g_cameraShakeImpulseMilli)*.001f;
}
