// Full same-frame packet preparation, opt-in. Device calls and publication
// stay on the owner. A late packet falls back immediately; it never stalls a draw.
#include "replay_packet_worker.h"
struct PacketPreparationState {
    d2vr_packet_prepare::Queue queue;
    std::shared_ptr<d2vr_packet_prepare::Job> job,spare;
    uint64_t ticket=0,hits=0,misses=0;
    double prepareMs=0;
    bool early=false;
};
static PacketPreparationState* g_packetPreparation=nullptr; // pinned process lifetime
static d2vr_packet_prepare::Shader SnapshotPacketShader(const VsInfo* vi) {
    d2vr_packet_prepare::Shader s;
    if(vi) {
        s.hash=vi->hash;s.wvpBase=vi->wvpBase;s.stpBase=vi->stpBase;
        s.reflectedWvp=vi->reflectedWvp;s.reflectedStp=vi->reflectedStp;
        s.reflectedPreviousWvp=vi->reflectedPreviousWvp;
        s.analyzed=vi->analyzed;s.reflected=vi->reflected;s.namedConstants=vi->namedConstants;
        s.activeWvp=ActiveWvpRegister(vi);s.activeStp=ActiveStpRegister(vi);
    }
    return s;
}
static bool SamePacketShader(const d2vr_packet_prepare::Shader& s,const VsInfo* vi) {
    return vi && s.hash==vi->hash && s.wvpBase==vi->wvpBase && s.stpBase==vi->stpBase &&
        s.reflectedWvp==vi->reflectedWvp && s.reflectedStp==vi->reflectedStp &&
        s.reflectedPreviousWvp==vi->reflectedPreviousWvp && s.analyzed==vi->analyzed &&
        s.reflected==vi->reflected && s.namedConstants==vi->namedConstants &&
        s.activeWvp==ActiveWvpRegister(vi) && s.activeStp==ActiveStpRegister(vi);
}
static void FinishPacketPreparation() {
    if(!g_packetPreparation || !g_packetPreparation->ticket)return;
    auto& state=*g_packetPreparation;
    state.job->cancel.store(true,std::memory_order_relaxed);
    LARGE_INTEGER before{},after{},frequency{};QueryPerformanceCounter(&before);
    const auto completed=state.queue.Wait(state.ticket);
    QueryPerformanceCounter(&after);QueryPerformanceFrequency(&frequency);
    static unsigned frames=0;static double prepareSum=0,joinSum=0;
    prepareSum+=state.prepareMs;joinSum+=1000.0*(after.QuadPart-before.QuadPart)/frequency.QuadPart;
    if(++frames==1 || frames%300==0)Log("[ReplayPackets] frames=%u hits=%llu fallback=%llu packets=%zu ready=%zu ownedBytes=%zu prepareAvgMs=%.4f joinAvgMs=%.4f; full packets, no FPS claim",
        frames,state.hits,state.misses,state.job->packets.size(),state.job->ready.load(),
        state.job->input.size()*sizeof(float),prepareSum/frames,joinSum/frames);
    if(!completed.complete || completed.firstFailure)g_fullPacketPrepareWorker=false;
    state.ticket=0;state.early=false;state.spare=std::move(state.job);
}
static void StartPacketPreparation(const StereoParams& sp,bool knownInitialShader=true) {
    FinishPacketPreparation();
    if(!g_fullPacketPrepareWorker || g_recOverflow || g_rec.size()>50000)return;
#ifdef D2VR_MATCHED_CAPTURE
    if(MatchedCaptureRecordingActive())return;
#endif
    LARGE_INTEGER before{};QueryPerformanceCounter(&before);
    try {
        auto job=g_packetPreparation && g_packetPreparation->spare ?
            std::move(g_packetPreparation->spare) : std::make_shared<d2vr_packet_prepare::Job>();
        job->Reset(g_rec.size());
        if((job->fp&_MCW_EM)!=_MCW_EM)return;
        job->initial=g_vsConstantHistory.begin;
        auto& p=job->params;
        p.separation=sp.separation;p.convergence=sp.convergence;p.mode=sp.mode;
        p.transX=sp.transX;p.transY=sp.transY;p.fovScaleX=sp.fovScaleX;p.fovScaleY=sp.fovScaleY;
        p.headGeometry=sp.headGeometry;memcpy(p.headClip,sp.headClip,sizeof(p.headClip));
        p.projectionFix=g_projectionFix;p.diagnosticSkipVs=g_diagnosticSkipVs;
        p.scale2DContent=g_scale2DContent;p.fixMotionVectorProjection=g_fixMotionVectorProjection;
        p.lightingPreset=g_lightingPreset;p.uiX=sp.fovScaleX;p.uiY=sp.fovScaleY;
#ifdef D2VR_MATCHED_CAPTURE
        NativeCameraUiScale(p.uiX,p.uiY);
#endif
        auto shader=g_activeVsOwner;
        // Before frame-start Apply the active shader belongs to live END.
        // Only explicit shader commands can certify early-prepared packets.
        if(!knownInitialShader)shader.reset();
        auto snapshot=SnapshotPacketShader(shader.get());
        unsigned useful=0;
        for(size_t i=0;i<g_rec.size();++i) {
            const auto& r=g_rec[i];
            if(r.op==OP_SetVertexShader) {
                shader=g_vsInfo.Lookup(reinterpret_cast<void*>(uintptr_t(r.a[0])));
                snapshot=SnapshotPacketShader(shader.get());continue;
            }
            d2vr_packet_prepare::Packet packet;
            if(r.op==OP_ApplyStateBlock) {
                shader.reset();packet.clear=true;
                if(!job->Add(i,packet,nullptr))return;
            } else if(r.op==OP_SetVSConstF) {
                if(r.a[0]>256 || r.a[1]>256-r.a[0] ||
                   uint64_t(r.a[4])+uint64_t(r.a[1])*16>g_recData.size())return;
                packet.first=r.a[0];packet.count=r.a[1];packet.known=bool(shader);
                packet.shader=snapshot;memcpy(&packet.px,&r.a[2],4);memcpy(&packet.py,&r.a[3],4);
                if(!job->Add(i,packet,reinterpret_cast<const float*>(g_recData.data()+r.a[4])))return;
                if(packet.known && packet.count<=68)++useful;
            }
        }
        if(useful<256){if(g_packetPreparation)g_packetPreparation->spare=std::move(job);return;}
        job->Seal();
        if(!g_packetPreparation) {
            HMODULE pinned=nullptr;
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&StartPacketPreparation),&pinned))return;
            g_packetPreparation=new PacketPreparationState;
        }
        auto& state=*g_packetPreparation;state.job=std::move(job);
        const auto submitted=state.queue.Submit(state.job);
        LARGE_INTEGER after{},frequency{};QueryPerformanceCounter(&after);QueryPerformanceFrequency(&frequency);
        state.prepareMs=1000.0*(after.QuadPart-before.QuadPart)/frequency.QuadPart;
        if(submitted.accepted){state.ticket=submitted.ticket;state.early=!knownInitialShader;}
        else state.job.reset();
    } catch(...) {FinishPacketPreparation();}
}
static bool EarlyPacketParametersMatch(const StereoParams& sp) {
    if(!g_packetPreparation || !g_packetPreparation->ticket || !g_packetPreparation->early)return false;
    const auto& p=g_packetPreparation->job->params;
    return p.mode==sp.mode && p.headGeometry==sp.headGeometry &&
        d2vr_prepare::Bits(p.separation,sp.separation) && d2vr_prepare::Bits(p.convergence,sp.convergence) &&
        d2vr_prepare::Bits(p.transX,sp.transX) && d2vr_prepare::Bits(p.transY,sp.transY) &&
        d2vr_prepare::Bits(p.fovScaleX,sp.fovScaleX) && d2vr_prepare::Bits(p.fovScaleY,sp.fovScaleY) &&
        !memcmp(p.headClip,sp.headClip,sizeof(p.headClip));
}
// Covers every error/early return after the sealed-frame prepass is launched.
// The worker owns its input; no D3D9/COM pointers or native object lists escape.
struct PacketPreparationLease {~PacketPreparationLease(){FinishPacketPreparation();}};
static const d2vr_packet_prepare::Output* PreparedVertexPacket(size_t record,bool complete) {
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_hook_cost::Scope hookCost(d2vr_hook_cost::PreparedPacket);
#endif
    if(!complete || !g_packetPreparation || !g_packetPreparation->ticket)return nullptr;
    auto& s=*g_packetPreparation;const auto& p=s.job->params;
    // Global hotkey/debug choices may change through a reentrant driver call.
    const auto* out=s.job->Try(record);
    float uiX=p.fovScaleX,uiY=p.fovScaleY;
#ifdef D2VR_MATCHED_CAPTURE
    if(out && out->effects.uiDependent)NativeCameraUiScale(uiX,uiY);
#endif
    if(!out || p.projectionFix!=g_projectionFix || p.diagnosticSkipVs!=g_diagnosticSkipVs ||
       p.scale2DContent!=(g_scale2DContent!=0) || p.fixMotionVectorProjection!=g_fixMotionVectorProjection ||
       p.lightingPreset!=g_lightingPreset ||
       (out->effects.uiDependent && (!d2vr_prepare::Bits(uiX,p.uiX) || !d2vr_prepare::Bits(uiY,p.uiY))) ||
       !SamePacketShader(s.job->packets[s.job->recordToPacket[record]].shader,g_activeVs)) {
        ++s.misses;return nullptr;
    }
    ++s.hits;return out;
}
