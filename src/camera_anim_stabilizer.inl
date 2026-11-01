// camera_anim_stabilizer.inl - remove first-person animation motion from the VR base camera.
//
// Evidence (docs/research/darkness2-camera-shake-20261101.md, headset traces 2026-11-01):
// the selected camera world matrix camera+0x80 follows the animated first-person rig
// (run bob up to ~0.1 units, 1-4 deg during tentacle/heart/melee animations) while the
// avatar parent world matrix parent+0x140 (class 00FFC300) carries the look yaw/pitch
// and agrees with the camera to ~0.1 deg at rest. Procedural shake owner 00FDF170 read
// zero throughout, so it is not the source.
//
// Diagnostics/CameraAnimStabilize=1 (default OFF) builds C_stab = parent rotation with
// the slowly averaged camera offset in parent space, then:
//   * view:  head delta H becomes D*H with D = C_anim * C_stab^-1 (row vectors,
//            view = C^-1), so the eye renders from C_stab;
//   * hands, gun model and shot ray use C_stab instead of camera+0x80,
// keeping world, arms and weapon in one frame. Large deviations (finishers, scripted
// cameras) and cinematics fade the correction out. No native memory is written.
struct CameraAnimStabilizerState {
    DWORD camera=0;
    bool valid=false,restValid=false,refusedLogged=false;
    float stable[16]{},delta[16]{},correction[16]{}; // correction E = C_stab * C_anim^-1
    float restLocal[3]{};
    // Adaptive (One Euro) filter on the stabilized camera: removes residual shake applied
    // to the avatar parent itself (tentacle hits, damage, train) without lag on real motion.
    bool filterValid=false;
    float filterPos[3]{},filterQuat[4]{0,0,0,1},rawPos[3]{},rawQuat[4]{0,0,0,1};
    float speed=0.f,angularSpeed=0.f,rumble=0.f,peakRumble=0.f,peakResidualDeg=0.f;
    float tiltAverage=0.f,peakTilt=0.f,residualAverage=0.f,residualPosAverage=0.f;
    // Snapshot taken once per tracked frame (UpdateHeadGeometry) so the view delta and the
    // hand/gun correction always come from the same update, whatever the call order.
    bool pubValid=false;DWORD pubCamera=0;ULONGLONG pubTime=0;
    float pubDelta[16]{},pubCorrection[16]{};
    // Timing witness: was the correction consumed in the frame that produced it?
    volatile LONG viewSame=0,viewStale=0,handSame=0,handStale=0;
    volatile LONG cameraMovedAfterCompute=0;
    float anim[16]{};
    float weight=0.f;
    ULONGLONG time=0,start=0,nextLog=0;
    unsigned long long frame=0,updates=0;
    float peakTranslation=0.f,peakAngle=0.f;
};
static CameraAnimStabilizerState g_cameraAnimStabilizer;
static SRWLOCK g_cameraAnimStabilizerLock=SRWLOCK_INIT;
static volatile LONG g_cameraShakeRumbleMilli=0;  // 0..1000, written on render thread
static volatile LONG g_cameraShakeRumbleTick=0;

namespace d2vr_camera_stabilizer {
inline float Dot3(const float* a,const float* b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline bool Normalize3(float* v){
    const float n=std::sqrt(Dot3(v,v));
    if(!std::isfinite(n) || n<1e-6f)return false;
    v[0]/=n;v[1]/=n;v[2]/=n;return true;
}
inline void Cross3(const float* a,const float* b,float* o){
    o[0]=a[1]*b[2]-a[2]*b[1];o[1]=a[2]*b[0]-a[0]*b[2];o[2]=a[0]*b[1]-a[1]*b[0];
}
// Orthonormal rows (right, up, forward) from a possibly scaled affine matrix.
inline bool Basis(const float m[16],float r[3][3]){
    float f[3]={m[8],m[9],m[10]},x[3]={m[0],m[1],m[2]},u[3];
    if(!Normalize3(f))return false;
    const float d=Dot3(x,f);
    for(int i=0;i<3;++i)x[i]-=d*f[i];
    if(!Normalize3(x))return false;
    Cross3(f,x,u);
    if(Dot3(u,m+4)<0){for(float& c:u)c=-c;} // keep the native up sign
    for(int i=0;i<3;++i){r[0][i]=x[i];r[1][i]=u[i];r[2][i]=f[i];}
    return true;
}
inline void ToQuat(const float r[3][3],float q[4]){
    const float t=r[0][0]+r[1][1]+r[2][2];
    if(t>0){float s=std::sqrt(t+1.f)*2.f;q[3]=.25f*s;q[0]=(r[2][1]-r[1][2])/s;q[1]=(r[0][2]-r[2][0])/s;q[2]=(r[1][0]-r[0][1])/s;}
    else if(r[0][0]>r[1][1]&&r[0][0]>r[2][2]){float s=std::sqrt(1.f+r[0][0]-r[1][1]-r[2][2])*2.f;q[3]=(r[2][1]-r[1][2])/s;q[0]=.25f*s;q[1]=(r[0][1]+r[1][0])/s;q[2]=(r[0][2]+r[2][0])/s;}
    else if(r[1][1]>r[2][2]){float s=std::sqrt(1.f+r[1][1]-r[0][0]-r[2][2])*2.f;q[3]=(r[0][2]-r[2][0])/s;q[0]=(r[0][1]+r[1][0])/s;q[1]=.25f*s;q[2]=(r[1][2]+r[2][1])/s;}
    else {float s=std::sqrt(1.f+r[2][2]-r[0][0]-r[1][1])*2.f;q[3]=(r[1][0]-r[0][1])/s;q[0]=(r[0][2]+r[2][0])/s;q[1]=(r[1][2]+r[2][1])/s;q[2]=.25f*s;}
}
inline void FromQuat(const float q[4],float r[3][3]){
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    r[0][0]=1-2*(y*y+z*z);r[0][1]=2*(x*y-z*w);r[0][2]=2*(x*z+y*w);
    r[1][0]=2*(x*y+z*w);r[1][1]=1-2*(x*x+z*z);r[1][2]=2*(y*z-x*w);
    r[2][0]=2*(x*z-y*w);r[2][1]=2*(y*z+x*w);r[2][2]=1-2*(x*x+y*y);
}
inline void Slerp(const float a[4],const float b0[4],float t,float o[4]){
    float b[4]={b0[0],b0[1],b0[2],b0[3]};
    float d=a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3];
    if(d<0){d=-d;for(float& c:b)c=-c;}
    float wa=1-t,wb=t;
    if(d<.9995f){const float th=std::acos((std::min)(1.f,d)),s=std::sin(th);wa=std::sin((1-t)*th)/s;wb=std::sin(t*th)/s;}
    float n=0;for(int i=0;i<4;++i){o[i]=wa*a[i]+wb*b[i];n+=o[i]*o[i];}
    n=std::sqrt(n);for(int i=0;i<4;++i)o[i]/=n;
}
inline float AngleDegrees(const float a[3][3],const float b[3][3]){
    // trace(A*B^T) for rotation rows
    float t=0;for(int i=0;i<3;++i)t+=Dot3(a[i],b[i]);
    const float c=(std::max)(-1.f,(std::min)(1.f,(t-1.f)*.5f));
    return std::acos(c)*57.29578f;
}
}

// One computation per tracked frame, serialized by the caller (EnsureCameraStabilizerFrame).
// The first consumer of a frame (either eye's view, hands or gun) triggers it, so both eyes
// and the hand/gun composition share one correction.
static void ComputeCameraStabilizer(DWORD camera,DWORD avatar) {
#ifdef D2VR_MATCHED_CAPTURE
    namespace s=d2vr_camera_stabilizer;
    if(!g_cameraAnimStabilize || !camera || !avatar || !g_trackedFrame.open)return;
    auto& st=g_cameraAnimStabilizer;
    float anim[16]{},root[16]{};
    __try {
        auto parent=**reinterpret_cast<BYTE***>(reinterpret_cast<BYTE*>(avatar)+0x1F8C);
        if(!parent || *reinterpret_cast<DWORD*>(parent)!=0xFFC300) {
            if(!st.refusedLogged){st.refusedLogged=true;Log("[CameraStabilize] REFUSED parent class; game camera unchanged");}
            AcquireSRWLockExclusive(&g_cameraAnimStabilizerLock);st.valid=false;ReleaseSRWLockExclusive(&g_cameraAnimStabilizerLock);
            return;
        }
        std::memcpy(anim,reinterpret_cast<const BYTE*>(camera)+0x80,sizeof(anim));
        std::memcpy(root,parent+0x140,sizeof(root));
    } __except(EXCEPTION_EXECUTE_HANDLER){return;}
    float ra[3][3],rr[3][3];
    for(float v:anim)if(!std::isfinite(v))return;
    for(float v:root)if(!std::isfinite(v))return;
    if(!s::Basis(anim,ra) || !s::Basis(root,rr))return;
    // VR: the headset owns pitch and roll. Keep only the parent heading so tentacle swings,
    // hits and the train cannot tilt the horizon; the parent tilt is measured for rumble.
    float rl[3][3];
    {
        float f[3]={rr[2][0],0.f,rr[2][2]},u[3]={0.f,rr[1][1]<0?-1.f:1.f,0.f},x[3];
        if(!s::Normalize3(f))return;
        s::Cross3(u,f,x);
        if(s::Dot3(x,rr[0])<0){for(float& c:x)c=-c;}
        for(int i=0;i<3;++i){rl[0][i]=x[i];rl[1][i]=u[i];rl[2][i]=f[i];}
    }
    const float tilt=s::AngleDegrees(rr,rl);
    const auto now=GetTickCount64();
    const float dt=st.time ? (std::min)(.1f,float(now-st.time)*.001f) : 0.f;
    const bool gap=!st.time || now-st.time>500 || st.camera!=camera;
    st.time=now;
    // Camera position in parent axes.
    float rel[3]={anim[12]-root[12],anim[13]-root[13],anim[14]-root[14]},local[3];
    for(int i=0;i<3;++i)local[i]=s::Dot3(rel,rl[i]);
    if(gap || !st.restValid){std::memcpy(st.restLocal,local,sizeof(local));st.restValid=true;st.weight=0.f;}
    // Stable candidate (full correction).
    float stablePos[3];
    for(int c=0;c<3;++c)stablePos[c]=root[12+c]+st.restLocal[0]*rl[0][c]+st.restLocal[1]*rl[1][c]+st.restLocal[2]*rl[2][c];
    float off[3]={anim[12]-stablePos[0],anim[13]-stablePos[1],anim[14]-stablePos[2]};
    const float translation=std::sqrt(s::Dot3(off,off)),angle=s::AngleDegrees(ra,rl);
    // Ordinary locomotion/attack animation is removed; finisher-scale or scripted moves fade out.
    float target=1.f;
    target=(std::min)(target,std::clamp((1.0f-translation)/0.5f,0.f,1.f));
    target=(std::min)(target,std::clamp((35.f-angle)/15.f,0.f,1.f));
    if(NativeCinematicPlaying())target=0.f;
    const float step=3.f*dt;
    st.weight+= std::clamp(target-st.weight,-step,step);
    // Average the rest offset only while the correction is engaged (1.5 s).
    if(target>=1.f && dt>0){const float a=1.f-std::exp(-dt/1.5f);for(int i=0;i<3;++i)st.restLocal[i]+=a*(local[i]-st.restLocal[i]);}
    // Blend: weight 1 = parent orientation + rest offset, 0 = native animated camera.
    float qa[4],qr[4],qt[4],pt[3];
    s::ToQuat(ra,qa);s::ToQuat(rl,qr);s::Slerp(qa,qr,st.weight,qt);
    for(int c=0;c<3;++c)pt[c]=anim[12+c]+st.weight*(stablePos[c]-anim[12+c]);
    // One Euro filter (cutoff rises with filtered speed): shake is high-frequency with a
    // near-zero mean velocity, locomotion and turning have a sustained velocity.
    float residualDeg=0.f,residualPos=0.f;
    if(gap || !st.filterValid || dt<=0.f) {
        std::memcpy(st.filterPos,pt,sizeof(pt));std::memcpy(st.filterQuat,qt,sizeof(qt));
        std::memcpy(st.rawPos,pt,sizeof(pt));std::memcpy(st.rawQuat,qt,sizeof(qt));
        st.speed=st.angularSpeed=0.f;st.filterValid=true;
    } else {
        const auto alpha=[dt](float cutoff){const float tau=1.f/(6.2831853f*cutoff);return 1.f/(1.f+tau/dt);};
        float d[3]={pt[0]-st.rawPos[0],pt[1]-st.rawPos[1],pt[2]-st.rawPos[2]};
        float rawRows[3][3],prevRows[3][3];s::FromQuat(qt,rawRows);s::FromQuat(st.rawQuat,prevRows);
        const float rawSpeed=std::sqrt(s::Dot3(d,d))/dt,rawAngular=s::AngleDegrees(rawRows,prevRows)/dt;
        const float ad=alpha(1.f);
        st.speed+=ad*(rawSpeed-st.speed);st.angularSpeed+=ad*(rawAngular-st.angularSpeed);
        const float ap=alpha(1.5f+1.5f*st.speed),aq=alpha(1.5f+0.06f*st.angularSpeed);
        for(int c=0;c<3;++c)st.filterPos[c]+=ap*(pt[c]-st.filterPos[c]);
        float fq[4];s::Slerp(st.filterQuat,qt,aq,fq);std::memcpy(st.filterQuat,fq,sizeof(fq));
        std::memcpy(st.rawPos,pt,sizeof(pt));std::memcpy(st.rawQuat,qt,sizeof(qt));
        float filtRows[3][3];s::FromQuat(st.filterQuat,filtRows);
        residualDeg=s::AngleDegrees(filtRows,rawRows);
        float rp[3]={pt[0]-st.filterPos[0],pt[1]-st.filterPos[1],pt[2]-st.filterPos[2]};
        residualPos=std::sqrt(s::Dot3(rp,rp));
    }
    // Final camera: filtered target, faded with the same weight as the animation removal.
    float q[4],rb[3][3],pos[3];
    s::Slerp(qt,st.filterQuat,st.weight,q);s::FromQuat(q,rb);
    for(int c=0;c<3;++c)pos[c]=pt[c]+st.weight*(st.filterPos[c]-pt[c]);
    float stable[16]{};
    for(int r=0;r<3;++r){
        const float scale=std::sqrt(s::Dot3(anim+4*r,anim+4*r)); // keep native row scale
        for(int c=0;c<3;++c)stable[4*r+c]=rb[r][c]*scale;
        stable[4*r+3]=anim[4*r+3];
    }
    for(int c=0;c<3;++c)stable[12+c]=pos[c];
    stable[15]=anim[15];
    float inverse[16],delta[16],inverseAnim[16],correction[16];
    if(!d2vr_math::Invert(stable,inverse) || !d2vr_math::Invert(anim,inverseAnim))return;
    d2vr_math::Multiply(anim,inverse,delta);
    d2vr_math::Multiply(stable,inverseAnim,correction);
    // Rumble only while standing: high-frequency parent tilt plus filtered heading jitter.
    // Walking/turning never rumbles (filter lag is not shake).
    {
        if(gap || dt<=0.f){st.tiltAverage=tilt;st.residualAverage=residualDeg;st.residualPosAverage=residualPos;}
        else {
            st.tiltAverage+=(1.f-std::exp(-dt/0.4f))*(tilt-st.tiltAverage);
            const float a=1.f-std::exp(-dt/0.12f);
            st.residualAverage+=a*(residualDeg-st.residualAverage);
            st.residualPosAverage+=a*(residualPos-st.residualPosAverage);
        }
        // High-passed only: stick turns and walking give a smooth filter lag, hits/blasts oscillate.
        const float shake=std::fabs(tilt-st.tiltAverage)+std::fabs(residualDeg-st.residualAverage)+
            20.f*std::fabs(residualPos-st.residualPosAverage);
        const float still=std::clamp((0.6f-st.speed)/0.3f,0.f,1.f)*std::clamp((40.f-st.angularSpeed)/20.f,0.f,1.f);
        const float raw=st.weight*still*std::clamp((shake-0.4f)/2.0f,0.f,1.f);
        st.rumble=raw>st.rumble ? raw : st.rumble+(raw-st.rumble)*(std::min)(1.f,dt*6.f);
        st.peakRumble=(std::max)(st.peakRumble,st.rumble);st.peakResidualDeg=(std::max)(st.peakResidualDeg,residualDeg);
        st.peakTilt=(std::max)(st.peakTilt,tilt);
        InterlockedExchange(&g_cameraShakeRumbleMilli,g_cameraShakeRumble?LONG(st.rumble*1000.f):0);
        InterlockedExchange(&g_cameraShakeRumbleTick,LONG(now&0x7fffffff));
    }
    AcquireSRWLockExclusive(&g_cameraAnimStabilizerLock);
    std::memcpy(st.stable,stable,sizeof(stable));std::memcpy(st.delta,delta,sizeof(delta));std::memcpy(st.correction,correction,sizeof(correction));
    st.camera=camera;st.frame=g_trackedFrame.id;st.valid=true;++st.updates;std::memcpy(st.anim,anim,sizeof(anim));
    ReleaseSRWLockExclusive(&g_cameraAnimStabilizerLock);
    if(!st.start){st.start=now;Log("[CameraStabilize] ACTIVE=1 camera=%08lX; view, hands, gun and shot use parent-stabilized camera; native memory untouched",camera);}
    st.peakTranslation=(std::max)(st.peakTranslation,translation*st.weight);
    st.peakAngle=(std::max)(st.peakAngle,angle*st.weight);
    if(now>=st.nextLog && now-st.start<180000) {
        st.nextLog=now+500;
        // Counter-prediction: removed* must be ~0 standing still and grow while running/attacking.
        Log("[CameraStabilize] ms=%llu updates=%llu weight=%.2f removedPos=%.4f removedDeg=%.2f peakPos=%.4f peakDeg=%.2f filterResidualPeakDeg=%.2f parentTiltPeak=%.2f speed=%.2f turn=%.1f rumblePeak=%.2f cameraMovedAfterCompute=%ld view(same/stale)=%ld/%ld hand(same/stale)=%ld/%ld rest=(%.4f,%.4f,%.4f) cinematic=%d",
            static_cast<unsigned long long>(now-st.start),st.updates,st.weight,translation*st.weight,angle*st.weight,
            st.peakTranslation,st.peakAngle,st.peakResidualDeg,st.peakTilt,st.speed,st.angularSpeed,st.peakRumble,InterlockedExchange(&st.cameraMovedAfterCompute,0),InterlockedExchange(&st.viewSame,0),InterlockedExchange(&st.viewStale,0),InterlockedExchange(&st.handSame,0),InterlockedExchange(&st.handStale,0),
            st.restLocal[0],st.restLocal[1],st.restLocal[2],int(NativeCinematicPlaying()));
        st.peakTranslation=st.peakAngle=st.peakRumble=st.peakResidualDeg=st.peakTilt=0.f;
    }
#else
    (void)camera;(void)avatar;
#endif
}
static SRWLOCK g_cameraStabilizerComputeLock=SRWLOCK_INIT;
static volatile LONG g_cameraStabilizerCamera=0,g_cameraStabilizerAvatar=0;
static void EnsureCameraStabilizerFrame() {
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_cameraAnimStabilize || !g_trackedFrame.open || !g_trackedFrame.id)return;
    const DWORD camera=DWORD(g_cameraStabilizerCamera),avatar=DWORD(g_cameraStabilizerAvatar);
    if(!camera || !avatar)return;
    if(!TryAcquireSRWLockExclusive(&g_cameraStabilizerComputeLock))return;
    const auto& st=g_cameraAnimStabilizer;
    if(!(st.valid && st.frame==g_trackedFrame.id && st.camera==camera))ComputeCameraStabilizer(camera,avatar);
    ReleaseSRWLockExclusive(&g_cameraStabilizerComputeLock);
#endif
}
static void SuppressCameraShakeSources();
static void ObserveSelectedCameraEvent(DWORD avatar);
// Selected-owner path (render thread): refresh the owner pointers, compute if this frame has
// no correction yet, and witness whether the native camera changed after an early compute.
static void UpdateCameraAnimationStabilizer(DWORD camera,DWORD avatar) {
#ifdef D2VR_MATCHED_CAPTURE
    ObserveSelectedCameraEvent(avatar);
    SuppressCameraShakeSources(); // native source first; independent INI switch
    if(!g_cameraAnimStabilize || !camera || !avatar)return;
    InterlockedExchange(&g_cameraStabilizerCamera,LONG(camera));
    InterlockedExchange(&g_cameraStabilizerAvatar,LONG(avatar));
    AcquireSRWLockExclusive(&g_cameraStabilizerComputeLock);
    auto& st=g_cameraAnimStabilizer;
    if(st.valid && st.frame==g_trackedFrame.id && st.camera==camera) {
        __try {
            if(std::memcmp(st.anim,reinterpret_cast<const BYTE*>(camera)+0x80,sizeof(st.anim)))
                InterlockedIncrement(&st.cameraMovedAfterCompute);
        } __except(EXCEPTION_EXECUTE_HANDLER){}
    } else ComputeCameraStabilizer(camera,avatar);
    ReleaseSRWLockExclusive(&g_cameraStabilizerComputeLock);
#else
    (void)camera;(void)avatar;
#endif
}
// Base camera for hand/gun/shot composition. Apply the cached correction E to the CURRENT
// native camera so a call before this frame's update does not lag locomotion by a frame.
static const float* StabilizedCameraWorld(const void* camera,float (&scratch)[16]) {
    const auto native=reinterpret_cast<const float*>(static_cast<const BYTE*>(camera)+0x80);
    if(!g_cameraAnimStabilize)return native;
    EnsureCameraStabilizerFrame();
    float correction[16];bool use=false;
    AcquireSRWLockShared(&g_cameraAnimStabilizerLock);
    const auto& st=g_cameraAnimStabilizer;
    if(st.valid && st.camera==DWORD(reinterpret_cast<uintptr_t>(camera)) && GetTickCount64()-st.time<100) {
        std::memcpy(correction,st.correction,sizeof(correction));use=true;
        InterlockedIncrement(st.frame==g_trackedFrame.id ? &g_cameraAnimStabilizer.handSame:&g_cameraAnimStabilizer.handStale);
    }
    ReleaseSRWLockShared(&g_cameraAnimStabilizerLock);
    if(!use)return native;
    float current[16];std::memcpy(current,native,sizeof(current)); // caller guards the read
    d2vr_math::Multiply(correction,current,scratch);
    return scratch;
}
// Controller rumble requested by removed shake (0..1), stale after 150 ms.
static float CameraShakeRumbleAmount() {
    if(!g_cameraAnimStabilize || !g_cameraShakeRumble)return 0.f;
    const LONG tick=g_cameraShakeRumbleTick;
    if(LONG(GetTickCount64()&0x7fffffff)-tick>150)return 0.f;
    return float(g_cameraShakeRumbleMilli)*.001f;
}
// D = C_anim * C_stab^-1 for the view head delta (identity when inactive).
static bool CameraStabilizerDelta(float out[16]) {
    if(!g_cameraAnimStabilize)return false;
    EnsureCameraStabilizerFrame();
    bool use=false;
    AcquireSRWLockShared(&g_cameraAnimStabilizerLock);
    const auto& st=g_cameraAnimStabilizer;
    if(st.valid && GetTickCount64()-st.time<100){
        std::memcpy(out,st.delta,16*sizeof(float));use=true;
        InterlockedIncrement(st.frame==g_trackedFrame.id ? &g_cameraAnimStabilizer.viewSame:&g_cameraAnimStabilizer.viewStale);
    }
    ReleaseSRWLockShared(&g_cameraAnimStabilizerLock);
    return use;
}
// Once per tracked frame, before the frame's head delta is frozen.
static void PublishCameraStabilizerFrame() {
    if(!g_cameraAnimStabilize)return;
    AcquireSRWLockExclusive(&g_cameraAnimStabilizerLock);
    auto& st=g_cameraAnimStabilizer;
    st.pubValid=st.valid;st.pubCamera=st.camera;st.pubTime=st.time;
    std::memcpy(st.pubDelta,st.delta,sizeof(st.pubDelta));
    std::memcpy(st.pubCorrection,st.correction,sizeof(st.pubCorrection));
    ReleaseSRWLockExclusive(&g_cameraAnimStabilizerLock);
}
