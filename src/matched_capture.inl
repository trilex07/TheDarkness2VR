// Compiled only with D2VR_MATCHED_CAPTURE. No GPU reads in the production build.
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
namespace matched_capture {
static uint64_t frame;
static bool active, failed;
static unsigned mask, draws[2], blits, inputs;
static uintptr_t sourceIdentities[2];
static unsigned projectionObserved;
static int lastSerial;
static std::string configToken;
static std::string ConfigToken() {
    char token[512];
    sprintf_s(token,"%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%.9g/%.9g/%.9g/%.9g/%d",
        g_submitMode,g_stereoMode,g_stereoDebugMode,g_stereoShiftMode,g_projectionFix,g_noInvProj,g_lightPosShift,g_skipLightVsShift,
        (int)g_headGeometry,g_stereoLayerPose,g_stereoPolarity,g_stereoIPD,g_gameUnitsPerMeter,g_fovWidenX,g_fovWidenY,g_lightingPreset);
    return token;
}
static std::string directory, iniPath;
static FILE *meta, *banks;
static void Reject(const char* why) {
    failed = true;
    if (meta) fprintf(meta, "{\"kind\":\"rejection\",\"reason\":\"%s\"}\n", why);
}
static bool Current() {
    if (!active) return false;
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
static void Finish(bool submitted) {
    if (!active) return;
    Current();
    const bool complete = !failed && submitted && mask==15 && inputs==3 && blits==3 && projectionObserved==3 && draws[0] && draws[1] && frame==g_trackedFrame.id;
    if (meta) fprintf(meta,"{\"kind\":\"frame_boundary\",\"frame\":%llu,\"present_counter\":%ld,\"pair_frame\":%llu,\"engine_frame_id\":null}\n",frame,InterlockedCompareExchange(&g_frameId,0,0),g_eyePairFrameId);
    if (banks && fclose(banks)!=0) failed=true;
    banks=nullptr;
    if (meta) {
        fprintf(meta,"{\"kind\":\"result\",\"frame\":%llu,\"complete\":%s,\"mask\":%u,\"draws\":[%u,%u]}\n",
            frame,complete&&!failed?"true":"false",mask,draws[0],draws[1]);
        const bool error=ferror(meta)!=0;
        const bool closed=fclose(meta)==0;
        if (error || !closed) failed=true;
        meta=nullptr;
    }
    if (complete && !failed) {
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
    Log("[MatchedCapture] frame=%llu complete=%d path=%s",frame,(int)(complete&&!failed),directory.c_str());
    active=false;
}
static void Begin() {
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
    if (!InterlockedCompareExchange(&g_stereoPresentBoundarySeen,0,0)) {
        Log("[MatchedCapture] rejected: no observed COM Present boundary; native dispatch patch is suppressed"); return;
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
    configToken=ConfigToken();
    frame=g_trackedFrame.id; active=true; failed=false; mask=0; blits=0; inputs=0; projectionObserved=0; draws[0]=draws[1]=0;
    if (!meta || !banks) { Reject("file creation failed"); Finish(false); return; }
    char dll[MAX_PATH]{}, exe[MAX_PATH]{}; HMODULE module=nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&Begin),&module) || !GetModuleFileNameA(module,dll,MAX_PATH) || !GetModuleFileNameA(nullptr,exe,MAX_PATH)) {
        Reject("module identity unavailable"); Finish(false); return;
    }
    const std::string dh=HashFile(dll), eh=HashFile(exe), ih=HashFile(iniPath.c_str()), rh=HashFile(request);
    fprintf(meta,"{\"kind\":\"header\",\"schema\":1,\"frame\":%llu,\"display_time\":%lld,\"dll_sha256\":\"%s\",\"exe_sha256\":\"%s\",\"ini_sha256\":\"%s\",\"request_sha256\":\"%s\",\"native_hooks_suppressed\":true,\"world_state\":\"render heuristic\",\"submit_mode\":%d,\"stereo_mode\":%d,\"shift_mode\":%d,\"layer_pose\":%d,\"stereo_baseline_game_units\":%.9g}\n",
        frame,g_trackedFrame.state.predictedDisplayTime,dh.c_str(),eh.c_str(),ih.c_str(),rh.c_str(),
        g_submitMode,g_stereoMode,g_stereoShiftMode,g_stereoLayerPose,g_stereoIPD);
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
    fprintf(meta,"]}\n"); const bool configError=ferror(meta)!=0;
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
static void Source(IDirect3DDevice9* dev,int eye) {
    if (!Current()) return;
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
            d2vr_replay::g_cameraPx,d2vr_replay::g_cameraPy,sp.fovScaleX,sp.fovScaleY,g_perEyeCropX[eye],g_perEyeCropY[eye]);
        Floats(sp.headClip,16); fprintf(meta,",\"inverse_projection_candidate\":"); Floats(d2vr_replay::g_cameraInverseProjection,16); fprintf(meta,"}\n");
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
static void MatchedCaptureDraw(IDirect3DDevice9* dev,int eye) {
    using namespace matched_capture;
    if (!Current()) return;
    if (d2vr_replay::g_replayEye<0 && !d2vr_replay::g_recActive) return;
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

static void MatchedProjectionObserved() {
    if (matched_capture::Current()) {
        const int eye=d2vr_replay::g_replayEye<0?0:d2vr_replay::g_replayEye;
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
