// Non-blocking D3D9 timestamp ring: elapsed marker span, NOT GPU Busy. Submission
// gaps and interleaved D3D11/sync can stretch it. No GetData(FLUSH) or readback.
struct GpuFrameTiming {
    struct Slot {
        IDirect3DQuery9 *start=nullptr,*mid=nullptr,*end=nullptr,*frequency=nullptr,*disjoint=nullptr;
        bool pending=false,midIssued=false;
    } slots[8];
    int active=-1,next=0;
    bool unavailable=false;
    double totalMs=0,maxMs=0;
    double liveMs=0,replayMs=0;
    unsigned splitSamples=0;
    unsigned samples=0;
    void Release() {
        for(auto& s:slots) {
            if(s.start) s.start->Release(); if(s.mid) s.mid->Release(); if(s.end) s.end->Release();
            if(s.frequency) s.frequency->Release(); if(s.disjoint) s.disjoint->Release();
            s={};
        }
        active=-1; next=0; unavailable=false;
    }
    void Poll() {
        for(auto& s:slots) if(s.pending) {
            UINT64 a=0,b=0,f=0; BOOL disjoint=TRUE;
            if(s.end->GetData(&b,sizeof(b),0)!=S_OK || s.start->GetData(&a,sizeof(a),0)!=S_OK ||
               s.frequency->GetData(&f,sizeof(f),0)!=S_OK || s.disjoint->GetData(&disjoint,sizeof(disjoint),0)!=S_OK) continue;
            s.pending=false;
            if(!disjoint && f && b>=a) {
                UINT64 mid=0;
                if(s.midIssued && s.mid && s.mid->GetData(&mid,sizeof(mid),0)==S_OK &&
                   mid>=a && mid<=b) {
                    liveMs+=(mid-a)*1000.0/f;
                    replayMs+=(b-mid)*1000.0/f;
                    ++splitSamples;
                }
                const double ms=(b-a)*1000.0/f;
                totalMs+=ms; maxMs=std::max(maxMs,ms); ++samples;
                if(samples>=120) {
                    Log("[GPUFrame] D3D9 live+replay mean=%.3fms max=%.3fms n=%u (timestamp span, includes submission gaps; NOT GPU Busy or full D3D11/XR time)",totalMs/samples,maxMs,samples);
                    if(splitSamples) Log("[GPUFrameSplit] live=%.3fms replay=%.3fms n=%u (timestamp spans, NOT GPU Busy)",
                        liveMs/splitSamples,replayMs/splitSamples,splitSamples);
                    samples=0; totalMs=maxMs=0;
                    splitSamples=0; liveMs=replayMs=0;
                }
            }
        }
    }
    void Begin(IDirect3DDevice9* dev) {
        if(!dev || unavailable || active>=0) return;
        Poll();
        Slot& s=slots[next];
        if(s.pending) return; // GPU behind: skip measurement instead of stalling
        if(!s.start) {
            if(FAILED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&s.start)) ||
               FAILED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&s.end)) ||
               FAILED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ,&s.frequency)) ||
               FAILED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT,&s.disjoint))) {
                Release(); unavailable=true; Log("[GPUFrame] timestamp queries unavailable"); return;
            }
        }
        if(g_gpuSplitTiming && !s.mid && FAILED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&s.mid))) {
            Log("[GPUFrameSplit] timestamp unavailable; total timing retained");
            g_gpuSplitTiming=false;
        }
        if(FAILED(s.disjoint->Issue(D3DISSUE_BEGIN))) return;
        if(FAILED(s.start->Issue(D3DISSUE_END))) { s.disjoint->Issue(D3DISSUE_END); return; }
        s.midIssued=false;
        active=next; next=(next+1)%8;
    }
    void MarkLiveEnd() {
        if(active>=0 && g_gpuSplitTiming && slots[active].mid)
            slots[active].midIssued=SUCCEEDED(slots[active].mid->Issue(D3DISSUE_END));
    }
    void End() {
        if(active<0) return;
        Slot& s=slots[active];
        const HRESULT a=s.end->Issue(D3DISSUE_END), b=s.frequency->Issue(D3DISSUE_END),c=s.disjoint->Issue(D3DISSUE_END);
        s.pending=SUCCEEDED(a) && SUCCEEDED(b) && SUCCEEDED(c); active=-1;
    }
};
GpuFrameTiming g_gpuTiming;
