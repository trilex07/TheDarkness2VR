// Non-blocking D3D9 timestamp ring. Covers live rendering + replay; D3D11 transfer
// and runtime compositor are excluded. No GetData(FLUSH), busy wait or readback.
struct GpuFrameTiming {
    struct Slot {
        IDirect3DQuery9 *start=nullptr,*end=nullptr,*frequency=nullptr,*disjoint=nullptr;
        bool pending=false;
    } slots[8];
    int active=-1,next=0;
    bool unavailable=false;
    double totalMs=0,maxMs=0;
    unsigned samples=0;
    void Release() {
        for(auto& s:slots) {
            if(s.start) s.start->Release(); if(s.end) s.end->Release();
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
                const double ms=(b-a)*1000.0/f;
                totalMs+=ms; maxMs=std::max(maxMs,ms); ++samples;
                if(samples>=120) {
                    Log("[GPUFrame] D3D9 live+replay mean=%.3fms max=%.3fms n=%u (excludes D3D11/XR)",totalMs/samples,maxMs,samples);
                    samples=0; totalMs=maxMs=0;
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
        if(FAILED(s.disjoint->Issue(D3DISSUE_BEGIN))) return;
        if(FAILED(s.start->Issue(D3DISSUE_END))) { s.disjoint->Issue(D3DISSUE_END); return; }
        active=next; next=(next+1)%8;
    }
    void End() {
        if(active<0) return;
        Slot& s=slots[active];
        const HRESULT a=s.end->Issue(D3DISSUE_END), b=s.frequency->Issue(D3DISSUE_END),c=s.disjoint->Issue(D3DISSUE_END);
        s.pending=SUCCEEDED(a) && SUCCEEDED(b) && SUCCEEDED(c); active=-1;
    }
};
GpuFrameTiming g_gpuTiming;
