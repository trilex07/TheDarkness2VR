#include "world_marker_projection.h"
#ifdef D2VR_MATCHED_CAPTURE
static d2vr_native_view::InstanceTable g_worldMarkerMovieTable;
using WorldMarkerProjectMethod=float*(__thiscall*)(void*,float*,const float*);
static WorldMarkerProjectMethod g_worldMarkerProjectOriginal=reinterpret_cast<WorldMarkerProjectMethod>(0x9F5220);
static std::atomic<uint64_t> g_worldMarkerCorrected{0},g_worldMarkerRefused{0};
static bool CorrectWorldMarkerProjection(void* movie,const float* world,float* output) {
    if(!g_worldObjectiveMarkers || !g_haveViews || !g_frameEyeParamsValid ||
       !g_trackedFrame.open || GetCurrentThreadId()!=g_renderThreadId ||
       g_nativeCoveredFrame!=g_trackedFrame.id || CinemaSceneThisFrame() ||
       !g_frameEyeParams[0].headGeometry || !g_worldMarkerMovieTable.Owns(movie))return false;
    __try {
        auto object=static_cast<BYTE*>(movie);
        auto context=**reinterpret_cast<BYTE***>(object+0x98);
        if(!context || *reinterpret_cast<DWORD*>(context)!=0xFE3EE8 || !context[0x90])return false;
        auto camera=**reinterpret_cast<BYTE***>(context+0x34);
        if(reinterpret_cast<uintptr_t>(camera)!=g_nativeCoveredCamera || !ProtectedAimCameraOwned(camera))return false;
        float native[16],rendered[16];
        std::memcpy(native,camera+0x500,sizeof(native));std::memcpy(rendered,native,sizeof(rendered));
        auto sp=g_frameEyeParams[0];
        // One native movie layout is shared by both eyes. Reproject its centre
        // using actual target depth; never pretend this adds per-eye disparity.
        sp.transX=sp.transY=sp.separation=0;sp.mode=0;
        d2vr_replay::ApplyEyeShiftToMatrix(rendered,4,sp,false);
        float uiX=1,uiY=1;
        if(g_scale2DContent){uiX=sp.fovScaleX;uiY=sp.fovScaleY;NativeCameraUiScale(uiX,uiY);}
        return d2vr_marker::Correct(native,rendered,world,uiX,uiY,
            float(*reinterpret_cast<int*>(object+0x68)),float(*reinterpret_cast<int*>(object+0x6C)),
            *reinterpret_cast<float*>(object+0x60),*reinterpret_cast<float*>(object+0x64),output);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static float* __fastcall WorldMarkerProjectHook(void* movie,void*,float* output,const float* world) {
    // Preserve the original ABI, native context selection and all error handling.
    float* result=g_worldMarkerProjectOriginal(movie,output,world);
    const bool corrected=result && world && CorrectWorldMarkerProjection(movie,world,result);
    auto& counter=corrected?g_worldMarkerCorrected:g_worldMarkerRefused;
    const auto number=counter.fetch_add(1,std::memory_order_relaxed)+1;
    if(number<=3 || number%600==0)
        Log("[WorldMarker] corrected=%d count=%llu frame=%llu open=%d covered=%llu thread=%lu renderThread=%lu head=%d; native movie projection only",
            int(corrected),number,g_trackedFrame.id,int(g_trackedFrame.open),g_nativeCoveredFrame,
            GetCurrentThreadId(),g_renderThreadId,int(g_frameEyeParamsValid && g_frameEyeParams[0].headGeometry));
    return result;
}
static bool WorldMarkerContracts() {
    static int contract=0;
    if(contract)return contract==1;
    struct Pin {uintptr_t address;size_t size;const char* digest;};
    const Pin pins[]={
        {0x9F5220,475,"a0a8fd3ffb85da152d368b63a8ab0b28c0f01d14e665db47fc35310bb718192f"},
        {0xFDA6E8,356,"e696ac1eca49217c27411713d4d581330ebf74748fd82133bb6c74e9abc7a9cf"},
        {0xC98BD0,257,"15ff2ef0033ce05e1253511291e3cfab855c2d941f741daa396475e96a1fe347"},
        {0xBB87C0,411,"48d140142551d971092fcc06d6abbbc0e45d2904da09f5c0e54e873269c42fa0"},
        {0x99BD60,80,"53ff1c5632c81eb4beb1ef955caf7444904eb6f0c77b87be51d6d3f1850e25bb"},
        {0xB2CAA0,7,"d18f3c193384b0b2cbc4f9da9c0425eda573d815b736310f49cb273f3e66b987"}};
    contract=-1;
    for(const auto& pin:pins)
        if(!d2vr_native_clone::MatchesDigest(reinterpret_cast<const void*>(pin.address),pin.size,pin.digest))return false;
    contract=g_worldMarkerMovieTable.Prepare(reinterpret_cast<void**>(0xFDA6E8),89,0x110/4,
        reinterpret_cast<void*>(&WorldMarkerProjectHook),g_nativeImageGuard)?1:-1;
    Log("[WorldMarker] contract=%s; private Movie vtable only, EXE and shared UI shader unchanged",contract==1?"admitted":"REFUSED");
    return contract==1;
}
// The admitted native window/render owner is also the movie list owner. Retain
// the same intrusive references used by native A0B120/9F5220 while borrowing.
static void* RetainWorldMarkerReference(void* reference) {
    __try {
        auto words=static_cast<DWORD*>(reference);
        if(!words || !words[0] || !words[1] || words[1]>1000000 ||
           !g_nativeImageGuard.Allows(reinterpret_cast<uintptr_t>(words+1),4))return nullptr;
        ++words[1];return reinterpret_cast<void*>(words[0]);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
static void PrepareWorldMarkerMoviesChecked() {
    auto managerRef=*reinterpret_cast<void**>(0x10B389C);
    auto manager=static_cast<BYTE*>(RetainWorldMarkerReference(managerRef));
    if(!manager)return;
    void* movieRefs[64]{};void* movies[64]{};unsigned count=0;
    __try {
        if(*reinterpret_cast<DWORD*>(manager)==0xFDA948) {
            auto begin=*reinterpret_cast<void***>(manager+0x58);
            auto end=*reinterpret_cast<void***>(manager+0x5C);
            if(end>=begin && end-begin<=64) {
                for(auto at=begin;at!=end;++at) {
                    auto ref=*at;auto movie=RetainWorldMarkerReference(ref);
                    if(movie){movieRefs[count]=ref;movies[count++]=movie;}
                }
            }
        }
        for(unsigned i=0;i<count;++i)g_worldMarkerMovieTable.Publish(movies[i],g_nativeImageGuard);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    for(unsigned i=0;i<count;++i)ReleaseSelectedLocalPlayer(uint32_t(reinterpret_cast<uintptr_t>(movieRefs[i])));
    ReleaseSelectedLocalPlayer(uint32_t(reinterpret_cast<uintptr_t>(managerRef)));
}
static void PrepareWorldMarkerMovies() {
    if(!g_worldObjectiveMarkers || !g_nativePlayerOwnerAdmitted || !g_nativeImageGuard.enabled ||
       !g_imagePreservingPresent || GetCurrentThreadId()!=g_renderThreadId ||
       !g_trackedFrame.open || g_submitMode!=4 || g_stereoMode!=1)return;
    DWORD process=0;
    if(GetWindowThreadProcessId(g_renderSizeHwnd,&process)!=GetCurrentThreadId() || process!=GetCurrentProcessId())return;
    if(!WorldMarkerContracts())return;
    static uint64_t frame=0;if(frame==g_trackedFrame.id)return;frame=g_trackedFrame.id;
    PrepareWorldMarkerMoviesChecked();
}
#else
static void PrepareWorldMarkerMovies() {}
#endif
