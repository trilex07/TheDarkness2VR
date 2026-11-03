// Included after the recorder and color history definitions. Only identities in
// this sealed frame are changed; payloads, opcodes and command order stay exact.
struct EyeOutputRecordPatch {
    struct Patch {size_t record;unsigned field;uint32_t before,after;};
    std::vector<Patch> changes;
    uint64_t generation=0;
    bool installed=false;
    static unsigned Fields(const d2vr_replay::Rec& r,unsigned fields[2],bool& texture){
        using namespace d2vr_replay;texture=false;
        switch(r.op){
        case OP_SetRenderTarget:fields[0]=1;return 1;
        case OP_SetTexture:texture=true;fields[0]=1;return 1;
        case OP_StretchRect:fields[0]=0;fields[1]=1;return 2;
        case OP_ColorFill:fields[0]=0;return 1;
        default:return 0;
        }
    }
    bool Begin(d2vr_eye_output::Output& out){
        using namespace d2vr_replay;
        RecLockGuard sealed;
        if(installed || !out.Active() || !out.Right() || !out.Source() || g_recActive || g_recOverflow || d2vr_record_sync::Busy() ||
            d2vr_record_sync::conflict.load() || g_rec.size()>50000)return false;
        changes.clear();generation=g_wvpHistoryGeneration;
        const auto source=uint32_t(uintptr_t(out.Source()));
        const auto tex=uint32_t(uintptr_t(out.SourceTexture()));
        try{
            for(size_t i=0;i<g_rec.size();++i){
                const auto& r=g_rec[i];
                if(r.op==OP_ApplyStateBlock ||
                    (r.op==OP_UpdateSurface && (r.a[0]==source || r.a[1]==source)) ||
                    (r.op==OP_UpdateTexture && tex && (r.a[0]==tex || r.a[1]==tex)) ||
                    (r.op==OP_SetRenderTarget && r.a[0]!=0 && r.a[1]==source))return false;
                unsigned fields[2];bool texture=false;
                const auto n=Fields(r,fields,texture);
                for(unsigned j=0;j<n;++j){
                    const auto f=fields[j],old=r.a[f];
                    const auto mapped=texture?uint32_t(uintptr_t(out.Texture(reinterpret_cast<IDirect3DBaseTexture9*>(uintptr_t(old))))):
                        uint32_t(uintptr_t(out.Surface(reinterpret_cast<IDirect3DSurface9*>(uintptr_t(old)))));
                    if(old!=mapped)changes.push_back({i,f,old,mapped});
                }
            }
        }catch(...){changes.clear();return false;}
        for(const auto& p:changes)g_rec[p.record].a[p.field]=p.after;
        installed=true;return true;
    }
    bool Restore(){
        if(!installed)return true;
        d2vr_replay::RecLockGuard sealed;
        bool exact=generation==d2vr_replay::g_wvpHistoryGeneration;
        for(const auto& p:changes)if(p.record>=d2vr_replay::g_rec.size() ||
            d2vr_replay::g_rec[p.record].a[p.field]!=p.after)exact=false;
        if(exact)for(const auto& p:changes)d2vr_replay::g_rec[p.record].a[p.field]=p.before;
        else d2vr_replay::g_recOverflow=true;
        installed=false;return exact;
    }
    ~EyeOutputRecordPatch(){Restore();}
};
