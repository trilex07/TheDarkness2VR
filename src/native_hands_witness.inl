// Matched-capture only. Compare the selected avatar renderer's already packed
// skin matrices with the D3D9 VS bank of the captured human-arm mesh candidate.
// This reads native memory but never changes a pose, render state, or game file.
struct MatchedPackedOverlap {
    size_t count=0;
    unsigned matches=0,unique=0,nontrivial=0;
    int pairGpu[12]{},pairCpu[12]{};
};
static MatchedPackedOverlap MatchedCountPackedOverlap(const float* packed,size_t count,const float* constants) {
    MatchedPackedOverlap out{};
    out.count=count;
    int distinctCpu[68]{};
    // BoneMatrices c45..c248 is a 68-matrix window. The selected renderer's
    // two packed buffers rotate independently of its three pose buffers.
    for(unsigned gi=0;gi<68;++gi) {
        const float* gpu=constants+(45+3*gi)*4;
        float magnitude=0,identityDelta=0;
        const float identity[12]={1,0,0,0,0,1,0,0,0,0,1,0};
        bool finite=true;
        for(int k=0;k<12;++k) {
            finite=finite && std::isfinite(gpu[k]);
            magnitude+=std::fabs(gpu[k]);
            identityDelta+=std::fabs(gpu[k]-identity[k]);
        }
        if(!finite || magnitude<.1f || identityDelta<.05f) continue;
        ++out.nontrivial;
        for(size_t pi=0;pi<count;++pi) {
            const float* cpu=packed+pi*12;
            bool same=true;
            for(int k=0;k<12;++k)
                if(!std::isfinite(cpu[k]) || std::fabs(cpu[k]-gpu[k])>1e-5f)
                    {same=false;break;}
            if(same) {
                if(out.matches<12) {out.pairGpu[out.matches]=(int)gi;out.pairCpu[out.matches]=(int)pi;}
                ++out.matches;
                bool unique=true;
                for(unsigned u=0;u<out.unique;++u)
                    if(distinctCpu[u]==(int)pi) {unique=false;break;}
                if(unique) distinctCpu[out.unique++]=(int)pi;
                break;
            }
        }
    }
    return out;
}
static void MatchedCaptureHandsWitness(IDirect3DVertexBuffer9* vb,IDirect3DIndexBuffer9* ib,
    UINT stride,const D3DVIEWPORT9& vp,unsigned shaderHash,const float* constants,int eye,unsigned draw) {
    if(!matched_capture::Current() || !vb || !ib || !constants || stride!=40 || vp.MaxZ>0.050001f ||
       (shaderHash!=0xC745D63Bu && shaderHash!=0x1CB7CEE2u)) return;
    D3DVERTEXBUFFER_DESC vd{}; D3DINDEXBUFFER_DESC id{};
    if(FAILED(vb->GetDesc(&vd)) || FAILED(ib->GetDesc(&id)) ||
       vd.Size!=114400 || id.Size!=85818) return;

    NativeGameplaySample sample;
    const bool fresh=CopyNativeGameplay(sample);
    const ULONGLONG sampleAge=fresh ? GetTickCount64()-sample.time:~0ULL;
    NativeHandsWriteStamp stamp{};
    AcquireSRWLockShared(&g_nativeHandsLock);
    stamp=g_nativeHandsLastWrite;
    ReleaseSRWLockShared(&g_nativeHandsLock);
    const char* status=fresh?"owner_rejected":"no_fresh_camera";
    const char* rightPose="no_write",*leftPose="no_write";
    void* renderer=nullptr;
    DWORD poseSlot=0,skinSlot=0;
    MatchedPackedOverlap overlap[2]{};
    bool packedValid[2]{};
    int dirty=-1;
    __try {
        if(fresh && NativeAimOwnerMatches((void*)sample.camera,0xFFC7C0)) {
            auto camera=(BYTE*)sample.camera;
            auto avatar=**(BYTE***)(camera+0x30);
            if(avatar && **(BYTE***)(avatar+0x630)==(BYTE*)sample.inventory) {
                auto parent=**(BYTE***)(avatar+0x1F8C);
                auto actor=parent && *(DWORD*)parent==0xFFC300 ? **(BYTE***)(parent+0xF8):nullptr;
                if(actor && *(DWORD*)actor==0xFC5F30) {
                    auto r=**(BYTE***)(actor+8);
                    auto resource=r ? **(BYTE***)(r+0x5BC):nullptr;
                    auto skeleton=resource && *(DWORD*)resource==0xFC2578 ?
                        **(BYTE***)(resource+0x5E0):nullptr;
                    if(skeleton && *(DWORD*)skeleton==0xFC2338) {
                        auto records=*(BYTE**)(skeleton+0x90);
                        if(*(BYTE**)(skeleton+0x94)-records==130*12 &&
                           !strcmp(*(char**)(records+12*5),"GAME_L1_ARM3") &&
                           !strcmp(*(char**)(records+12*27),"GAME_R1_ARM3")) {
                            poseSlot=*(DWORD*)(r+0x424);
                            skinSlot=*(DWORD*)(r+0x448);
                            if(poseSlot<3 && skinSlot<2) {
                                dirty=(int)r[0x580+skinSlot];
                                if(stamp.sequence) {
                                    const char* initial=stamp.frame==matched_capture::frame ?
                                        "different_renderer":"older_frame";
                                    rightPose=leftPose=initial;
                                    if(stamp.frame==matched_capture::frame && stamp.renderer==r) {
                                        rightPose=leftPose="slot_rotated";
                                        if(stamp.poseSlot==poseSlot) {
                                            auto poseVec=r+0x3F4+16*poseSlot;
                                            auto pose=*(float**)poseVec;
                                            const uintptr_t pb=(uintptr_t)pose,pe=(uintptr_t)*(float**)(poseVec+4);
                                            rightPose=leftPose="pose_replaced";
                                            if(pose==stamp.palette && pb && pe-pb==130*64) {
                                                rightPose=!stamp.rightWritten ? "not_written":
                                                    memcmp(pose+27*16,stamp.rightWrist,sizeof(stamp.rightWrist))==0 ?
                                                        "survived":"changed_after_write";
                                                leftPose=!stamp.leftWritten ? "not_written":
                                                    memcmp(pose+5*16,stamp.leftWrist,sizeof(stamp.leftWrist))==0 ?
                                                        "survived":"changed_after_write";
                                            }
                                        }
                                    }
                                }
                                renderer=r;
                                for(DWORD slot=0;slot<2;++slot) {
                                    auto vec=r+0x428+16*slot;
                                    auto begin=*(float**)vec;
                                    auto end=*(float**)(vec+4);
                                    const uintptr_t b=(uintptr_t)begin,e=(uintptr_t)end;
                                    if(b && e>=b && e-b<=512*48 && (e-b)%48==0 && !(b&15)) {
                                        packedValid[slot]=true;
                                        overlap[slot]=MatchedCountPackedOverlap(begin,(e-b)/48,constants);
                                    }
                                }
                                status=packedValid[skinSlot]?"compared":"invalid_or_large_packed_vector";
                            } else status="invalid_slots";
                        } else status="wrong_skeleton";
                    } else status="wrong_resource";
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        status="native_read_exception";renderer=nullptr;
        packedValid[0]=packedValid[1]=false;overlap[0]={};overlap[1]={};
    }
    const auto& active=overlap[skinSlot<2?skinSlot:0];
    const auto& alternate=overlap[skinSlot<2?1-skinSlot:1];
    fprintf(matched_capture::meta,
        "{\"kind\":\"avatar_palette_overlap\",\"frame\":%llu,\"eye\":%d,\"draw\":%u,"
        "\"vb\":\"%p\",\"ib\":\"%p\",\"renderer\":\"%p\",\"status\":\"%s\","
        "\"sample_age_ms\":%llu,\"pose_slot\":%lu,\"skin_slot\":%lu,\"dirty\":%d,"
        "\"packed_count\":%u,\"nontrivial_gpu\":%u,\"matches\":%u,\"unique_matches\":%u,"
        "\"alternate_packed_valid\":%s,\"alternate_packed_count\":%u,"
        "\"alternate_matches\":%u,\"alternate_unique_matches\":%u,"
        "\"cpu_write_seq\":%ld,\"write_frame\":%llu,\"write_age_ms\":%llu,"
        "\"right_pose\":\"%s\",\"left_pose\":\"%s\",\"pairs\":[",
        matched_capture::frame,eye,draw,vb,ib,renderer,status,
        sampleAge,poseSlot,skinSlot,dirty,(unsigned)active.count,active.nontrivial,active.matches,active.unique,
        packedValid[skinSlot<2?1-skinSlot:1]?"true":"false",(unsigned)alternate.count,
        alternate.matches,alternate.unique,
        stamp.sequence,stamp.frame,stamp.sequence ? GetTickCount64()-stamp.time:~0ULL,
        rightPose,leftPose);
    for(unsigned i=0;i<active.matches && i<12;++i)
        fprintf(matched_capture::meta,"%s[%d,%d]",i?",":"",active.pairGpu[i],active.pairCpu[i]);
    fputs("]}\n",matched_capture::meta);
}
