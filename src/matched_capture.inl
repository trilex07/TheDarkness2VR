// Compiled only with D2VR_MATCHED_CAPTURE. No GPU reads in the production build.
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
namespace matched_capture {
static uint64_t frame;
static bool active, failed;
static bool pendingPresent;
static uint64_t presentSerial, presentEpoch, presentFrame;
static uintptr_t presentDevice;
static LONG resetGeneration;
static unsigned mask, draws[2], blits, inputs;
static uintptr_t sourceIdentities[2];
static unsigned projectionObserved;
static std::vector<uintptr_t> geometrySaved;
// Optional one-frame pass localization. No reads or allocations when disabled.
static bool passStages, postStagesOnly, postInputsSaved;
static IDirect3DSurface9* stageTarget;
static unsigned stagesSaved, stageLastDraw, stageVs, stagePs;
static uint64_t stageBytes;
static std::unordered_set<uint64_t> shadersSaved;
static void FlushStage(IDirect3DDevice9* dev);
static void ReleaseStage() { if(stageTarget) stageTarget->Release(); stageTarget=nullptr; }
static float nativeProjection[2][16]{};
static int lastSerial;
static std::string configToken;
static std::string ConfigToken() {
    char token[512];
    sprintf_s(token,"%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%.9g/%.9g/%.9g/%.9g/%d",
        g_submitMode,g_stereoMode,g_stereoDebugMode,g_stereoShiftMode,g_projectionFix,g_noInvProj,g_lightPosShift,g_skipLightVsShift,
        (int)g_headGeometry,g_stereoLayerPose,g_stereoPolarity,g_stereoIPD,g_gameUnitsPerMeter,g_fovWidenX,g_fovWidenY,g_lightingPreset);
    return std::string(token)+"/"+(g_matchEyeResolutionToSource?"1":"0")+"/"+(g_worldDiffuseAnisotropy?"1":"0")+"/"+std::to_string(g_textureAnisotropy);
}
static std::string directory, iniPath;
static FILE *meta, *banks;
static void Reject(const char* why) {
    failed = true;
    if (meta) fprintf(meta, "{\"kind\":\"rejection\",\"reason\":\"%s\"}\n", why);
}
static bool Current() {
    if (!active) return false;
    if (!g_capturePresent.InCaptureWindow(presentSerial,presentEpoch,presentFrame,GetCurrentThreadId(),g_frameId) ||
        g_captureResetting || resetGeneration!=g_captureResetGeneration || presentDevice!=g_capturePresent.device) {
        Reject("capture lost its current Present witness or reset generation"); return false;
    }
    if (frame != g_trackedFrame.id || !g_trackedFrame.open) {
        Reject("mixed or closed XR frame"); return false;
    }
    if (ConfigToken()!=configToken) { Reject("effective render settings changed during capture"); return false; }
    return !failed;
}
static std::string HashFile(const char* path) {
    FILE* f = nullptr; fopen_s(&f, path, "rb");
    if (!f) return "unavailable";
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32], buffer[65536]; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        size_t n; ok = true;
        while ((n = fread(buffer, 1, sizeof(buffer), f)) != 0)
            if (BCryptHashData(hash, buffer, (ULONG)n, 0) < 0) { ok = false; break; }
        ok = ok && !ferror(f) && BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    }
    fclose(f); if (hash) BCryptDestroyHash(hash); if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) return "unavailable";
    char text[65]{}; for (int i=0;i<32;++i) sprintf_s(text+2*i,3,"%02x",digest[i]);
    return text;
}
static void Floats(const float* v, unsigned n) {
    fputc('[',meta);
    for (unsigned i=0;i<n;++i) {
        if (i) fputc(',',meta);
        if (std::isfinite(v[i])) fprintf(meta,"%.9g",v[i]);
        else { fputs("null",meta); failed=true; }
    }
    fputc(']',meta);
}
static void Pose(const XrPosef& p) {
    float values[]={p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z};
    Floats(values,7);
}
// Called only AFTER the observed original COM Present returns. A suppressed,
// failed or occluded call cannot publish a COMPLETE receipt.
static void PresentReturned(HRESULT hr, bool forwarded) {
    const bool same=g_capturePresent.Matches(presentSerial,presentEpoch,presentFrame,GetCurrentThreadId()) &&
        !g_captureResetting && resetGeneration==g_captureResetGeneration && presentFrame==(uint64_t)g_frameId && presentDevice==g_capturePresent.device;
    if (g_presentReadiness && g_capturePresent.active &&
        (g_capturePresent.serial==1 || g_capturePresent.serial%1000==0 || pendingPresent))
        Log("[PresentReadiness] return serial=%llu epoch=%llu frame=%llu forwarded=%d hr=0x%08lX",
            g_capturePresent.serial,g_capturePresent.epoch,g_capturePresent.frame,(int)forwarded,hr);
    if (!pendingPresent) {
        // An arm at the tail of this call must not survive a failed/suppressed
        // arming boundary. No files can become COMPLETE from that request.
        if (active && (!forwarded || hr!=D3D_OK)) Reject("arming Present did not complete successfully");
        return;
    }
    pendingPresent=false;
    const bool completed=same && forwarded && hr==D3D_OK;
    FILE* append=nullptr;
    fopen_s(&append,(directory+"/manifest.jsonl").c_str(),"ab");
    if (!append) { failed=true; return; }
    fprintf(append,"{\"kind\":\"present_completion\",\"frame\":%llu,\"serial\":%llu,\"epoch\":%llu,\"game_frame\":%llu,\"same_witness\":%s,\"forwarded\":%s,\"hr\":%ld}\n",
        frame,presentSerial,presentEpoch,presentFrame,same?"true":"false",forwarded?"true":"false",hr);
    fprintf(append,"{\"kind\":\"result\",\"frame\":%llu,\"complete\":%s,\"mask\":%u,\"draws\":[%u,%u]}\n",
        frame,completed?"true":"false",mask,draws[0],draws[1]);
    const bool error=ferror(append)!=0;
    if (fclose(append)!=0 || error) failed=true;
    if (completed && !failed) {
        FILE* receipt=nullptr;
        fopen_s(&receipt,(directory+"/COMPLETE.json").c_str(),"wb");
        if (receipt) {
            const char* names[]={"manifest.jsonl","vs-banks.bin","source-eye0.bmp","source-eye1.bmp","final-eye0.bmp","final-eye1.bmp","effective-config.json"};
            fputc('{',receipt);
            for (int i=0;i<7;++i) {
                const std::string h=HashFile((directory+"/"+names[i]).c_str());
                if (h=="unavailable") failed=true;
                fprintf(receipt,"%s\"%s\":\"%s\"",i?",":"",names[i],h.c_str());
            }
            fputs("}\n",receipt); if (fclose(receipt)!=0) failed=true;
        } else failed=true;
        if (failed) DeleteFileA((directory+"/COMPLETE.json").c_str());
    }
    Log("[MatchedCapture] frame=%llu complete=%d path=%s",frame,(int)(completed&&!failed),directory.c_str());
}
static void Finish(bool submitted) {
    if (!active) return;
    Current();
    ReleaseStage();
    const bool complete = !failed && submitted && mask==15 && inputs==3 && blits==3 && projectionObserved==3 && draws[0] && draws[1] && frame==g_trackedFrame.id;
    if (meta) fprintf(meta,"{\"kind\":\"frame_boundary\",\"frame\":%llu,\"present_counter\":%ld,\"pair_frame\":%llu,\"engine_frame_id\":null}\n",frame,InterlockedCompareExchange(&g_frameId,0,0),g_eyePairFrameId);
    if (banks && fclose(banks)!=0) failed=true;
    banks=nullptr;
    if (meta) {
        if(passStages) fprintf(meta,"{\"kind\":\"pass_stage_summary\",\"frame\":%llu,\"attempts\":%u,\"bytes_budgeted\":%llu,\"attempt_limit\":8,\"byte_limit\":268435456}\n",frame,stagesSaved,stageBytes);
        fprintf(meta,"{\"kind\":\"staged_result\",\"frame\":%llu,\"complete\":%s,\"mask\":%u,\"draws\":[%u,%u]}\n",
            frame,complete&&!failed?"true":"false",mask,draws[0],draws[1]);
        const bool error=ferror(meta)!=0;
        const bool closed=fclose(meta)==0;
        if (error || !closed) failed=true;
        meta=nullptr;
    }
    pendingPresent=complete && !failed;
    Log("[MatchedCapture] frame=%llu staged=%d awaiting-original-Present-return path=%s",frame,(int)pendingPresent,directory.c_str());
    active=false;
}
static void Begin() {
    if (pendingPresent) return; // do not overwrite a staged capture before its return
    if (active) { Reject("frame expired before complete submission"); Finish(false); }
    // A serial requests ONE next heuristically loaded world frame, never a menu.
    static ULONGLONG polled;
    const ULONGLONG now=GetTickCount64();
    if (now-polled<1000) return; polled=now;
    const char* request=".\\Darkness2VR-diagnostic.ini";
    if (!GetPrivateProfileIntA("Diagnostics","MatchedFrameCapture",0,request)) return;
    const int serial=GetPrivateProfileIntA("Diagnostics","MatchedCaptureSerial",0,request);
    if (serial<=0 || serial==lastSerial || !g_in3DGameplayMode) return;
    lastSerial=serial; // consume even unsupported/failed attempts; no automatic retry
    if (!g_capturePresent.active || g_captureResetting || g_capturePresent.frame!=(uint64_t)g_frameId ||
        g_capturePresent.thread!=GetCurrentThreadId()) {
        Log("[MatchedCapture] rejected: no current COM Present witness; prior Present or EndScene is insufficient"); return;
    }
    if (g_submitMode!=4 || g_stereoMode!=1 || g_stereoDebugMode!=0) {
        Log("[MatchedCapture] rejected request: requires SubmitMode=4 StereoMode=1 StereoDebugMode=0"); return;
    }
    char temp[MAX_PATH], name[100];
    const DWORD tempLen=GetTempPathA(MAX_PATH,temp);
    if (!tempLen || tempLen>=MAX_PATH) { Log("[MatchedCapture] rejected temp path"); return; }
    sprintf_s(name,"Darkness2VR-matched-%lu-%d-%llu",GetCurrentProcessId(),serial,g_trackedFrame.id);
    directory=std::string(temp)+name;
    if (!CreateDirectoryA(directory.c_str(),nullptr)) { Log("[MatchedCapture] directory creation rejected"); return; }
    fopen_s(&meta,(directory+"/manifest.jsonl").c_str(),"wb");
    fopen_s(&banks,(directory+"/vs-banks.bin").c_str(),"wb");
    configToken=ConfigToken(); geometrySaved.clear();
    ReleaseStage(); stagesSaved=0; stageBytes=0; shadersSaved.clear();
    passStages=GetPrivateProfileIntA("Diagnostics","CapturePassStages",0,request)!=0;
    postStagesOnly=GetPrivateProfileIntA("Diagnostics","CapturePassStages",0,request)==2;
    postInputsSaved=false;
    frame=g_trackedFrame.id; active=true; failed=false; mask=0; blits=0; inputs=0; projectionObserved=0; draws[0]=draws[1]=0;
    pendingPresent=false; presentSerial=g_capturePresent.serial+1; presentEpoch=g_capturePresent.epoch;
    presentFrame=g_capturePresent.frame+1; resetGeneration=g_captureResetGeneration;
    presentDevice=g_capturePresent.device;
    if (!meta || !banks) { Reject("file creation failed"); Finish(false); return; }
    char dll[MAX_PATH]{}, exe[MAX_PATH]{}; HMODULE module=nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&Begin),&module) || !GetModuleFileNameA(module,dll,MAX_PATH) || !GetModuleFileNameA(nullptr,exe,MAX_PATH)) {
        Reject("module identity unavailable"); Finish(false); return;
    }
    const std::string dh=HashFile(dll), eh=HashFile(exe), ih=HashFile(iniPath.c_str()), rh=HashFile(request);
    fprintf(meta,"{\"kind\":\"header\",\"schema\":2,\"frame\":%llu,\"display_time\":%lld,\"dll_sha256\":\"%s\",\"exe_sha256\":\"%s\",\"ini_sha256\":\"%s\",\"request_sha256\":\"%s\",\"native_hooks_suppressed\":true,\"world_state\":\"render heuristic\",\"submit_mode\":%d,\"stereo_mode\":%d,\"shift_mode\":%d,\"layer_pose\":%d,\"stereo_baseline_game_units\":%.9g}\n",
        frame,g_trackedFrame.state.predictedDisplayTime,dh.c_str(),eh.c_str(),ih.c_str(),rh.c_str(),
        g_submitMode,g_stereoMode,g_stereoShiftMode,g_stereoLayerPose,g_stereoIPD);
    fprintf(meta,"{\"kind\":\"capture_arm\",\"frame\":%llu,\"expected_serial\":%llu,\"epoch\":%llu,\"expected_game_frame\":%llu,\"reset_generation\":%ld,\"thread\":%lu,\"device\":\"%p\"}\n",
        frame,presentSerial,presentEpoch,presentFrame,resetGeneration,GetCurrentThreadId(),reinterpret_cast<void*>(presentDevice));
    FILE* saved=meta; FILE* config=nullptr;
    fopen_s(&config,(directory+"/effective-config.json").c_str(),"wb");
    if (!config) { Reject("effective config output failed"); return; }
    meta=config;
    fprintf(meta,"{\"submit_mode\":%d,\"stereo_mode\":%d,\"debug_mode\":%d,\"shift_mode\":%d,\"projection_fix\":%d,\"no_inv_proj\":%d,\"light_pos_shift\":%d,\"skip_light_vs_shift\":%d,\"head_geometry\":%d,\"layer_pose\":%d,\"baseline_game_units\":%.9g,\"game_units_per_meter\":%.9g,\"eye_params\":[",
        g_submitMode,g_stereoMode,g_stereoDebugMode,g_stereoShiftMode,g_projectionFix,g_noInvProj,g_lightPosShift,g_skipLightVsShift,(int)g_headGeometry,g_stereoLayerPose,g_stereoIPD,g_gameUnitsPerMeter);
    for(int eye=0;eye<2;++eye) {
        const auto& sp=g_frameEyeParams[eye];
        fprintf(meta,"%s{\"head_geometry\":%s,\"mode\":%d,\"values\":[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g],\"head_clip\":",eye?",":"",sp.headGeometry?"true":"false",sp.mode,sp.separation,sp.convergence,sp.transX,sp.transY,sp.fovScaleX,sp.fovScaleY,(double)g_stereoPolarity);
        Floats(sp.headClip,16); fprintf(meta,",\"inverse_head_clip\":"); Floats(sp.inverseHeadClip,16); fputc('}',meta);
    }
    fprintf(meta,"],\"fix_motion_vector_projection\":%s,\"production_resolution_requested\":%s,\"client_rect_hooked\":%s,\"requested_render_size\":[%d,%d],\"match_eye_resolution_to_source\":%s,\"xr_recommended_size\":[%u,%u],\"xr_max_both_eyes\":[%u,%u],\"world_diffuse_anisotropy\":%s,\"texture_anisotropy\":%d}\n",
        g_fixMotionVectorProjection?"true":"false",g_diagnosticProductionResolution?"true":"false",
        g_getClientRectHooked?"true":"false",g_renderW,g_renderH,g_matchEyeResolutionToSource?"true":"false",
        g_steamVrRecW,g_steamVrRecH,g_xrMaxEyeW,g_xrMaxEyeH,g_worldDiffuseAnisotropy?"true":"false",g_textureAnisotropy); const bool configError=ferror(meta)!=0;
    const bool configClosed=fclose(meta)==0; meta=saved;
    if(configError || !configClosed) Reject("effective config write failed");
    const std::string ch=HashFile((directory+"/effective-config.json").c_str());
    const auto& l=g_eyePose[0].position; const auto& r=g_eyePose[1].position;
    const float dx=l.x-r.x,dy=l.y-r.y,dz=l.z-r.z;
    fprintf(meta,"{\"kind\":\"config_identity\",\"frame\":%llu,\"effective_config_sha256\":\"%s\",\"xr_eye_separation_m\":%.9g}\n",frame,ch.c_str(),std::sqrt(dx*dx+dy*dy+dz*dz));
    if (dh=="unavailable" || eh=="unavailable" || ih=="unavailable" || rh=="unavailable") Reject("identity hash unavailable");
}
static bool Bmp(const char* name, UINT w, UINT h, const void* data, UINT pitch, bool rgba) {
    if (!w || !h || w>16384 || h>16384) return false;
    FILE* f=nullptr; fopen_s(&f,(directory+"/"+name).c_str(),"wb"); if (!f) return false;
    BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
    fh.bfType=0x4d42; fh.bfOffBits=sizeof(fh)+sizeof(ih); fh.bfSize=fh.bfOffBits+w*h*4;
    ih.biSize=sizeof(ih); ih.biWidth=w; ih.biHeight=-(LONG)h; ih.biPlanes=1; ih.biBitCount=32;
    bool ok=fwrite(&fh,sizeof(fh),1,f)==1 && fwrite(&ih,sizeof(ih),1,f)==1;
    std::vector<unsigned char> row(w*4);
    for (UINT y=0;y<h && ok;++y) {
        memcpy(row.data(),(const unsigned char*)data+y*pitch,w*4);
        if (rgba) for (UINT x=0;x<w;++x) std::swap(row[4*x],row[4*x+2]);
        ok=fwrite(row.data(),row.size(),1,f)==1;
    }
    const bool closed=fclose(f)==0; return ok&&closed;
}
static const char* SaveStageSurface(IDirect3DDevice9* dev,IDirect3DSurface9* target,unsigned draw,
                             unsigned vs,unsigned ps,const char* phase,int sampler=-1) {
    if(!passStages || !target || stagesSaved>=8) return "unavailable_or_budget";
    D3DSURFACE_DESC d{};
    if(FAILED(target->GetDesc(&d))) return "get_desc_failed";
    if(d.Width<(UINT)std::max(512,g_renderW/2) || d.Height<(UINT)std::max(512,g_renderH/2) ||
        (uint64_t)d.Width*d.Height>16u*1024u*1024u) return "unsupported_dimensions";
    if(d.MultiSampleType!=D3DMULTISAMPLE_NONE) return "multisampled";
    unsigned bytesPerPixel=0;
    if(d.Format==D3DFMT_A8R8G8B8 || d.Format==D3DFMT_X8R8G8B8 || d.Format==D3DFMT_G16R16F || d.Format==D3DFMT_R32F) bytesPerPixel=4;
    if(d.Format==D3DFMT_A16B16G16R16F) bytesPerPixel=8;
    const uint64_t size=(uint64_t)d.Width*d.Height*bytesPerPixel;
    if(!size) return "unsupported_format";
    if(size>256u*1024u*1024u-stageBytes) return "byte_budget";
    ++stagesSaved; stageBytes+=size; // attempts bounded too, including failures
    IDirect3DSurface9* sys=nullptr; D3DLOCKED_RECT lock{};
    HRESULT hr=dev->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,&sys,nullptr);
    if(SUCCEEDED(hr)) hr=dev->GetRenderTargetData(target,sys);
    if(SUCCEEDED(hr)) hr=sys->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    char name[64]; sprintf_s(name,"stage-%02u-draw-%u.%s",stagesSaved,draw,
        d.Format==D3DFMT_A8R8G8B8 || d.Format==D3DFMT_X8R8G8B8?"bmp":"bin");
    bool saved=false;
    if(SUCCEEDED(hr)) {
        if(d.Format==D3DFMT_A8R8G8B8 || d.Format==D3DFMT_X8R8G8B8)
            saved=Bmp(name,d.Width,d.Height,lock.pBits,lock.Pitch,false);
        else {
            FILE* f=nullptr;
            if(!fopen_s(&f,(directory+"/"+name).c_str(),"wb") && f) {
                saved=true;
                for(UINT y=0;y<d.Height;++y)
                    if(fwrite((unsigned char*)lock.pBits+y*lock.Pitch,bytesPerPixel,d.Width,f)!=d.Width) {saved=false;break;}
                if(fclose(f)!=0) saved=false;
            }
        }
        sys->UnlockRect();
    }
    if(sys) sys->Release();
    fprintf(meta,"{\"kind\":\"pass_stage\",\"frame\":%llu,\"eye\":0,\"last_draw\":%u,\"rt\":\"%p\",\"width\":%u,\"height\":%u,\"format\":%u,\"vs_hash\":%u,\"ps_hash\":%u,\"phase\":\"%s\",\"sampler\":%d,\"file\":\"%s\",\"saved\":%s,\"hr\":%ld,\"sha256\":\"%s\"}\n",
        frame,draw,target,d.Width,d.Height,(unsigned)d.Format,vs,ps,phase,sampler,name,
        saved?"true":"false",hr,saved?HashFile((directory+"/"+name).c_str()).c_str():"unavailable");
    return saved?"saved":"readback_failed";
}
static bool IsPostStage(unsigned ps) {
    return ps==0x33DE98C1u || ps==0x75F88E11u || ps==0x309E99B6u || ps==0x2B17CF23u ||
           ps==0x120C78F8u || ps==0x5104FEF7u;
}
static void FlushStage(IDirect3DDevice9* dev) {
    if(postStagesOnly && !IsPostStage(stagePs)) return;
    SaveStageSurface(dev,stageTarget,stageLastDraw,stageVs,stagePs,"after_draw");
}
static void CapturePostInputs(IDirect3DDevice9* dev,unsigned draw,unsigned vs,unsigned ps) {
    if(!postStagesOnly || postInputsSaved || (ps!=0x33DE98C1u && ps!=0x75F88E11u)) return;
    postInputsSaved=true;
    float values[16*4]{};
    const HRESULT hr=dev->GetPixelShaderConstantF(0,values,16);
    fprintf(meta,"{\"kind\":\"post_constants\",\"frame\":%llu,\"eye\":0,\"draw\":%u,\"ps_hash\":%u,\"hr\":%ld,\"c0_to_c15\":",frame,draw,ps,hr);
    Floats(values,64);fprintf(meta,"}\n");
    // Exact composite's CTAB: Frame=s0, Depth=s2, DOFScene=s3. Read its inputs
    // before the draw; never infer them from an opaque texture pointer alone.
    for(unsigned slot:{0u,2u,3u}) {
        IDirect3DBaseTexture9* base=nullptr;IDirect3DSurface9* surface=nullptr;
        D3DSURFACE_DESC desc{};
        const HRESULT textureHr=dev->GetTexture(slot,&base);
        const unsigned type=base?(unsigned)base->GetType():0;
        HRESULT surfaceHr=E_FAIL,descHr=E_FAIL;
        const char* status=FAILED(textureHr)?"get_texture_failed":!base?"unbound":type!=D3DRTYPE_TEXTURE?"non_2d":"get_surface_failed";
        if(SUCCEEDED(textureHr) && base && type==D3DRTYPE_TEXTURE) {
            surfaceHr=static_cast<IDirect3DTexture9*>(base)->GetSurfaceLevel(0,&surface);
            if(SUCCEEDED(surfaceHr) && surface) {
                descHr=surface->GetDesc(&desc);
                status=FAILED(descHr)?"get_desc_failed":SaveStageSurface(dev,surface,draw,vs,ps,"before_draw_input",slot);
            }
        }
        // Even unsupported depth formats must leave an identity/status record.
        // No readback/conversion is invented when the format cannot be copied.
        fprintf(meta,"{\"kind\":\"post_input\",\"frame\":%llu,\"eye\":0,\"draw\":%u,\"ps_hash\":%u,\"sampler\":%u,\"texture\":\"%p\",\"texture_type\":%u,\"surface\":\"%p\",\"surface_available\":%s,\"texture_hr\":%ld,\"surface_hr\":%ld,\"desc_hr\":%ld,\"width\":%u,\"height\":%u,\"format\":%u,\"usage\":%u,\"pool\":%u,\"multisample\":%u,\"readback_status\":\"%s\"}\n",
            frame,draw,ps,slot,base,type,surface,SUCCEEDED(descHr)?"true":"false",textureHr,surfaceHr,descHr,
            desc.Width,desc.Height,(unsigned)desc.Format,desc.Usage,(unsigned)desc.Pool,(unsigned)desc.MultiSampleType,status);
        if(surface)surface->Release();if(base)base->Release();
    }
}
template<class Shader> static void SaveStageShader(Shader* shader,unsigned declaredHash,bool pixel) {
    if(!passStages || !shader || shadersSaved.size()>=32) return;
    const uint64_t key=((uint64_t)pixel<<32)|declaredHash;
    if(!shadersSaved.insert(key).second) return;
    UINT size=0;
    if(FAILED(shader->GetFunction(nullptr,&size)) || !size || size>128*1024 || size%4) return;
    std::vector<DWORD> code(size/4);
    if(FAILED(shader->GetFunction(code.data(),&size))) return;
    const unsigned actual=d2vr_replay::HashBytes(code.data(),size);
    char name[48];sprintf_s(name,"%s-%08X.bin",pixel?"ps":"vs",actual);
    FILE* f=nullptr;bool ok=false;
    if(!fopen_s(&f,(directory+"/"+name).c_str(),"wb") && f) {
        ok=fwrite(code.data(),1,size,f)==size; if(fclose(f)!=0) ok=false;
    }
    fprintf(meta,"{\"kind\":\"stage_shader\",\"frame\":%llu,\"pixel\":%s,\"declared_hash\":%u,\"actual_hash\":%u,\"file\":\"%s\",\"saved\":%s,\"sha256\":\"%s\"}\n",
        frame,pixel?"true":"false",declaredHash,actual,name,ok?"true":"false",ok?HashFile((directory+"/"+name).c_str()).c_str():"unavailable");
}
static void Source(IDirect3DDevice9* dev,int eye) {
    if (!Current()) return;
    if (eye==0) {
        FlushStage(dev); ReleaseStage();
        if (!g_capturePresent.Matches(presentSerial,presentEpoch,presentFrame,GetCurrentThreadId())) {
            Reject("source has no matching closing Present"); return;
        }
        fprintf(meta,"{\"kind\":\"present_entry\",\"frame\":%llu,\"serial\":%llu,\"epoch\":%llu,\"game_frame\":%llu,\"reset_generation\":%ld,\"thread\":%lu,\"device\":\"%p\",\"caller\":\"%p\",\"boundary\":\"%s\",\"engine_ownership\":\"UNKNOWN\"}\n",
            frame,presentSerial,presentEpoch,presentFrame,resetGeneration,GetCurrentThreadId(),
            reinterpret_cast<void*>(g_capturePresent.device),reinterpret_cast<void*>(g_capturePresent.caller),g_capturePresent.kind);
    }
    if (!(projectionObserved&(1u<<eye))) { Reject("no same-eye current-frame inverse projection witness"); return; }
    if (!g_had3DCameraThisFrame) { Reject("no current world camera witness"); return; }
    if (mask&(1u<<eye)) { Reject("duplicate source eye"); return; }
    IDirect3DSurface9 *rt=nullptr,*sys=nullptr; D3DSURFACE_DESC d{}; D3DVIEWPORT9 vp{}; D3DLOCKED_RECT lock{};
    bool ok=SUCCEEDED(dev->GetRenderTarget(0,&rt)) && rt && SUCCEEDED(rt->GetDesc(&d)) &&
        SUCCEEDED(dev->GetViewport(&vp)) && (uint64_t)d.Width*d.Height<=16u*1024u*1024u && d.MultiSampleType==D3DMULTISAMPLE_NONE &&
        (d.Format==D3DFMT_A8R8G8B8 || d.Format==D3DFMT_X8R8G8B8) &&
        SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,&sys,nullptr)) &&
        SUCCEEDED(dev->GetRenderTargetData(rt,sys)) && SUCCEEDED(sys->LockRect(&lock,nullptr,D3DLOCK_READONLY));
    if (ok) {
        sourceIdentities[eye]=(uintptr_t)rt;
        char name[32]; sprintf_s(name,"source-eye%d.bmp",eye);
        ok=Bmp(name,d.Width,d.Height,lock.pBits,lock.Pitch,false); sys->UnlockRect();
        const auto& sp=g_frameEyeParams[eye];
        fprintf(meta,"{\"kind\":\"source\",\"frame\":%llu,\"eye\":%d,\"rt\":\"%p\",\"width\":%u,\"height\":%u,\"format\":%u,\"viewport\":[%u,%u,%u,%u,%.9g,%.9g],\"native_px_py_candidate\":[%.9g,%.9g],\"scale\":[%.9g,%.9g],\"crop\":[%.9g,%.9g],\"head_clip\":",
            frame,eye,rt,d.Width,d.Height,(unsigned)d.Format,vp.X,vp.Y,vp.Width,vp.Height,vp.MinZ,vp.MaxZ,
            1.f/nativeProjection[eye][0],1.f/nativeProjection[eye][5],sp.fovScaleX,sp.fovScaleY,g_perEyeCropX[eye],g_perEyeCropY[eye]);
        Floats(sp.headClip,16); fprintf(meta,",\"inverse_projection_candidate\":"); Floats(nativeProjection[eye],16); fprintf(meta,"}\n");
    }
    if (sys) sys->Release(); if (rt) rt->Release();
    if (!ok) Reject("source RT0 readback failed or unsupported format/MSAA"); else mask|=1u<<eye;
}
static void Final(int eye,ID3D11Texture2D* image) {
    if (!Current()) return;
    if (!image || (mask&(4u<<eye))) { Reject("missing or duplicate final eye"); return; }
    D3D11_TEXTURE2D_DESC d{}; image->GetDesc(&d);
    const bool rgba=d.Format==DXGI_FORMAT_R8G8B8A8_UNORM || d.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if ((uint64_t)d.Width*d.Height>16u*1024u*1024u || (!rgba && d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) || d.SampleDesc.Count!=1 || d.ArraySize!=1 || d.MipLevels!=1) {
        Reject("unsupported final format/layout"); return;
    }
    ID3D11Texture2D* staging=nullptr; auto sd=d; sd.Usage=D3D11_USAGE_STAGING; sd.BindFlags=0; sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ; sd.MiscFlags=0;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    bool ok=SUCCEEDED(g_dev11->CreateTexture2D(&sd,nullptr,&staging));
    if (ok) { g_ctx11->CopyResource(staging,image); ok=SUCCEEDED(g_ctx11->Map(staging,0,D3D11_MAP_READ,0,&mapped)); }
    if (ok) {
        char name[32]; sprintf_s(name,"final-eye%d.bmp",eye);
        ok=Bmp(name,d.Width,d.Height,mapped.pData,mapped.RowPitch,rgba); g_ctx11->Unmap(staging,0);
        fprintf(meta,"{\"kind\":\"final\",\"frame\":%llu,\"eye\":%d,\"rt\":\"%p\",\"width\":%u,\"height\":%u,\"format\":%u}\n",frame,eye,image,d.Width,d.Height,(unsigned)d.Format);
    }
    if (staging) staging->Release(); if (!ok) Reject("final readback failed"); else mask|=4u<<eye;
}
static void Submission(const XrCompositionLayerProjectionView* views) {
    if (!Current()) return;
    for (int eye=0;eye<2;++eye) {
        fprintf(meta,"{\"kind\":\"submission\",\"frame\":%llu,\"eye\":%d,\"fov_l_r_u_d\":",frame,eye);
        const auto& f=views[eye].fov; float angles[]={f.angleLeft,f.angleRight,f.angleUp,f.angleDown}; Floats(angles,4);
        fprintf(meta,",\"pose_xyzw_xyz\":"); Pose(views[eye].pose);
        fprintf(meta,",\"xr_located_eye_pose\":"); Pose(g_pairSubmitEyePose[eye]);
        fprintf(meta,",\"rect\":[%d,%d,%d,%d]}\n",views[eye].subImage.imageRect.offset.x,views[eye].subImage.imageRect.offset.y,views[eye].subImage.imageRect.extent.width,views[eye].subImage.imageRect.extent.height);
    }
}
} // namespace matched_capture
static void MatchedCaptureDraw(IDirect3DDevice9* dev,int eye,bool worldDiffuseApplied) {
    using namespace matched_capture;
    if (d2vr_replay::g_replayEye<0 && !d2vr_replay::g_recActive) return;
    if (!Current()) return;
    if (eye<0 || eye>1 || draws[eye]>=16384) { Reject("draw budget or eye invalid"); return; }
    float constants[256*4]{}; D3DVIEWPORT9 vp{}; IDirect3DSurface9* rt=nullptr; IDirect3DVertexShader9* vs=nullptr;
    bool ok=SUCCEEDED(dev->GetVertexShaderConstantF(0,constants,256)) && SUCCEEDED(dev->GetViewport(&vp)) &&
        SUCCEEDED(dev->GetRenderTarget(0,&rt)) && SUCCEEDED(dev->GetVertexShader(&vs));
    const long offset=ftell(banks);
    ok=ok && offset>=0 && fwrite(constants,sizeof(constants),1,banks)==1;
    int wvp=-1,stp=-1; unsigned hash=0;
    auto it=d2vr_replay::g_vsInfo.find(vs);
    if (it!=d2vr_replay::g_vsInfo.end()) { wvp=it->second.wvpBase; stp=it->second.stpBase; hash=it->second.hash; }
    fprintf(meta,"{\"kind\":\"draw\",\"frame\":%llu,\"eye\":%d,\"draw\":%u,\"rt\":\"%p\",\"vs\":\"%p\",\"shader_hash\":%u,\"wvp_register\":%d,\"stp_register\":%d,\"bank_offset\":%ld,\"viewport\":[%u,%u,%u,%u,%.9g,%.9g]}\n",
        frame,eye,draws[eye]++,rt,vs,hash,wvp,stp,offset,vp.X,vp.Y,vp.Width,vp.Height,vp.MinZ,vp.MaxZ);
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DPixelShader9* ps=nullptr;
    IDirect3DBaseTexture9* texture=nullptr;UINT streamOffset=0,stride=0;
    const bool geometry=SUCCEEDED(dev->GetStreamSource(0,&vb,&streamOffset,&stride)) &&
        SUCCEEDED(dev->GetIndices(&ib)) && SUCCEEDED(dev->GetPixelShader(&ps)) && SUCCEEDED(dev->GetTexture(0,&texture));
    const auto psHash=d2vr_replay::g_psHashByPtr.find(ps);
    const unsigned pixelHash=psHash==d2vr_replay::g_psHashByPtr.end()?0:psHash->second;
    if (d2vr_sampling::IsWorldDiffuseMaterial(hash,pixelHash)) {
        DWORD min=0,mag=0,mip=0,quality=0,bias=0;
        const bool sampler=SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MINFILTER,&min)) &&
            SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag)) &&
            SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MIPFILTER,&mip)) &&
            SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MAXANISOTROPY,&quality)) &&
            SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,&bias));
        fprintf(meta,"{\"kind\":\"material_sampler\",\"frame\":%llu,\"eye\":%d,\"draw\":%u,\"valid\":%s,\"min\":%lu,\"mag\":%lu,\"mip\":%lu,\"max_anisotropy\":%lu,\"lod_bias_bits\":%lu,\"scoped_fix_enabled\":%s,\"scoped_fix_applied\":%s}\n",
            frame,eye,draws[eye]-1,sampler?"true":"false",min,mag,mip,quality,bias,g_worldDiffuseAnisotropy?"true":"false",worldDiffuseApplied?"true":"false");
    }
    if(eye==0 && passStages) {
        if(stageTarget!=rt) {
            FlushStage(dev); ReleaseStage(); stageTarget=rt; if(stageTarget) stageTarget->AddRef();
        }
        stageLastDraw=draws[eye]-1; stageVs=hash; stagePs=pixelHash;
        if(!postStagesOnly || IsPostStage(pixelHash) || d2vr_sampling::IsWorldDiffuseMaterial(hash,pixelHash)) {
            SaveStageShader(vs,hash,false); SaveStageShader(ps,pixelHash,true);
        }
        CapturePostInputs(dev,draws[eye]-1,hash,pixelHash);
    }
    DWORD cull=0,scissor=0;RECT clip{};
    dev->GetRenderState(D3DRS_CULLMODE,&cull);dev->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor);dev->GetScissorRect(&clip);
    fprintf(meta,"{\"kind\":\"draw_geometry\",\"frame\":%llu,\"eye\":%d,\"draw\":%u,\"valid\":%s,\"vb\":\"%p\",\"ib\":\"%p\",\"ps\":\"%p\",\"texture0\":\"%p\",\"stream_offset\":%u,\"stride\":%u,\"cull\":%lu,\"scissor\":%lu,\"clip\":[%ld,%ld,%ld,%ld]}\n",
        frame,eye,draws[eye]-1,geometry?"true":"false",vb,ib,ps,texture,streamOffset,stride,cull,scissor,clip.left,clip.top,clip.right,clip.bottom);
    fprintf(meta,"{\"kind\":\"draw_ps\",\"frame\":%llu,\"eye\":%d,\"draw\":%u,\"shader_hash\":%u}\n",frame,eye,draws[eye]-1,pixelHash);
    // One bounded read-only snapshot of the first-person buffers. Private output,
    // never included in a commit; no locks outside an explicit capture request.
    if (eye==0 && vp.MaxZ<=0.05f && vb && ib && geometrySaved.size()<8 &&
        std::find(geometrySaved.begin(),geometrySaved.end(),(uintptr_t)vb)==geometrySaved.end()) {
        geometrySaved.push_back((uintptr_t)vb);
        D3DVERTEXBUFFER_DESC vd{}; D3DINDEXBUFFER_DESC id{};
        auto save=[&](const char* kind,void* identity,void* data,UINT size) {
            char name[80];sprintf_s(name,"%s-%p.bin",kind,identity);FILE* f=nullptr;
            if(!fopen_s(&f,(directory+"/"+name).c_str(),"wb") && f) { fwrite(data,1,size,f);fclose(f); }
        };
        void* data=nullptr;
        if(SUCCEEDED(vb->GetDesc(&vd)) && vd.Size<=4*1024*1024 && SUCCEEDED(vb->Lock(0,0,&data,D3DLOCK_READONLY))) {
            save("vb",vb,data,vd.Size);vb->Unlock();
        }
        if(SUCCEEDED(ib->GetDesc(&id)) && id.Size<=1024*1024 && SUCCEEDED(ib->Lock(0,0,&data,D3DLOCK_READONLY))) {
            save("ib",ib,data,id.Size);ib->Unlock();
        }
        IDirect3DVertexDeclaration9* decl=nullptr;D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]{};UINT n=MAXD3DDECLLENGTH+1;
        if(SUCCEEDED(dev->GetVertexDeclaration(&decl)) && decl) {
            if(SUCCEEDED(decl->GetDeclaration(elements,&n))) save("decl",vb,elements,n*sizeof(elements[0]));
            decl->Release();
        }
    }
    if(vb) vb->Release();if(ib) ib->Release();if(ps) ps->Release();if(texture) texture->Release();
    D3DMATRIX transforms[3]{};
    const bool ff=SUCCEEDED(dev->GetTransform(D3DTS_WORLD,&transforms[0])) &&
        SUCCEEDED(dev->GetTransform(D3DTS_VIEW,&transforms[1])) && SUCCEEDED(dev->GetTransform(D3DTS_PROJECTION,&transforms[2]));
    fprintf(meta,"{\"kind\":\"fixed_function\",\"frame\":%llu,\"eye\":%d,\"draw\":%u,\"valid\":%s,\"world_view_projection\":",frame,eye,draws[eye]-1,ff?"true":"false");
    if(ff) Floats(reinterpret_cast<const float*>(transforms),48); else fputs("null",meta);
    fprintf(meta,"}\n");
    if (!vs && !ff) ok=false;
    DWORD srgb=0, samplerSrgb[16]{};
    bool colorOk=SUCCEEDED(dev->GetRenderState(D3DRS_SRGBWRITEENABLE,&srgb));
    for (unsigned i=0;i<16;++i) colorOk=SUCCEEDED(dev->GetSamplerState(i,D3DSAMP_SRGBTEXTURE,&samplerSrgb[i])) && colorOk;
    fprintf(meta,"{\"kind\":\"color_state\",\"frame\":%llu,\"eye\":%d,\"draw\":%u,\"srgb_write\":%lu,\"sampler_srgb\":[",frame,eye,draws[eye]-1,srgb);
    for(unsigned i=0;i<16;++i) fprintf(meta,"%s%lu",i?",":"",samplerSrgb[i]); fprintf(meta,"]}\n");
    if(!colorOk) ok=false;
    if (rt) rt->Release(); if (vs) vs->Release(); if (!ok) Reject("draw state readback failed");
}

static void MatchedProjectionObserved(const float* native,const float* uploaded,UINT start,UINT count,int base,HRESULT hr) {
    if (matched_capture::Current() && d2vr_capture::ProjectionUpload(native,uploaded,start,count,base,hr)) {
        const int eye=d2vr_replay::g_replayEye<0?0:d2vr_replay::g_replayEye;
        if(eye>1) {matched_capture::Reject("invalid projection eye");return;}
        const unsigned offset=(base-start)*4;
        memcpy(matched_capture::nativeProjection[eye],native+offset,16*sizeof(float));
        fprintf(matched_capture::meta,"{\"kind\":\"projection_upload\",\"frame\":%llu,\"eye\":%d,\"register\":%d,\"provenance\":\"%s\",\"native_candidate\":",
            matched_capture::frame,eye,base,eye==0?"live_PS_upload":"recorded_replay_PS_upload");
        matched_capture::Floats(native+offset,16);
        fprintf(matched_capture::meta,",\"uploaded\":");matched_capture::Floats(uploaded+offset,16);fprintf(matched_capture::meta,"}\n");
        matched_capture::projectionObserved|=1u<<eye;
    }
}
static void MatchedCaptureBlit(int eye,const StereoCBData& cb) {
    using namespace matched_capture;
    if (!Current()) return;
    if (blits&(1u<<eye)) { Reject("duplicate blit"); return; }
    blits|=1u<<eye;
    fprintf(meta,"{\"kind\":\"blit\",\"frame\":%llu,\"eye\":%d,\"viewport\":[0,0,%u,%u,0,1],\"cb_floats\":",frame,eye,g_scW,g_scH);
    Floats(reinterpret_cast<const float*>(&cb),sizeof(cb)/sizeof(float));
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{}; D3D11_RENDER_TARGET_VIEW_DESC rtv{}; D3D11_SAMPLER_DESC sampler{};
    if(!g_uploadSRV || !g_eyeRTV[eye] || !g_stereoSamp11) { Reject("missing blit resource state"); return; }
    g_uploadSRV->GetDesc(&srv); g_eyeRTV[eye]->GetDesc(&rtv); g_stereoSamp11->GetDesc(&sampler);
    fprintf(meta,",\"srv_format\":%u,\"rtv_format\":%u,\"sampler_filter\":%u,\"address_uv\":[%u,%u]}\n",(unsigned)srv.Format,(unsigned)rtv.Format,(unsigned)sampler.Filter,(unsigned)sampler.AddressU,(unsigned)sampler.AddressV);
}

static void MatchedCaptureInput(IDirect3DSurface9* source,bool fallback) {
    using namespace matched_capture;
    if (!Current()) return;
    const int eye=d2vr_replay::g_replayEye<0?0:d2vr_replay::g_replayEye;
    // ReplayStream resets g_replayEye before its output is uploaded. Source mask
    // is a monotonic eye boundary here, rather than relying on that draw marker.
    const int uploadEye=(mask&2)?1:eye;
    if (fallback || (uintptr_t)source!=sourceIdentities[uploadEye] || (inputs&(1u<<uploadEye))) {
        Reject("production input differs from captured RT0 or duplicate input"); return;
    }
    inputs|=1u<<uploadEye;
    fprintf(meta,"{\"kind\":\"input\",\"frame\":%llu,\"eye\":%d,\"rt\":\"%p\"}\n",frame,uploadEye,source);
}
