// Each slot owns its full producer/reader lifetime. Fixed eye slots remove
// the left-reader -> right-write dependency without reusing a pending input.
struct SharedCaptureSlot {
    IDirect3DTexture9* tex9=nullptr;
    IDirect3DSurface9* surface9=nullptr;
    ID3D11Texture2D* tex11=nullptr;
    ID3D11ShaderResourceView* srv11=nullptr;
    IDirect3DQuery9* producer=nullptr;
    ID3D11Query* reader=nullptr;
    HANDLE handle=nullptr;
    UINT width=0,height=0;
    d2vr_shared_reader::Fence fence;
    std::shared_ptr<d2vr_transfer::Job> deferred;
};
static SharedCaptureSlot g_sharedCapture[2];
int g_zeroCopyFailures=0;
static d2vr_transfer::Queue* g_stereoTransferQueue=nullptr;
static DWORD g_stereoTransferOwner=0;
static IDirect3DDevice9* g_stereoTransferDevice=nullptr;
static bool g_stereoTransferFault=false;
static bool g_splitCaptureUnavailable=false;
struct SharedReaderFenceOps {
    SharedCaptureSlot& slot;
    bool Valid() const {return g_ctx11 && slot.reader;}
    const void* Context() const {return g_ctx11;}
    const void* Query() const {return slot.reader;}
    uint32_t Thread() const {return GetCurrentThreadId();}
    void End() {g_ctx11->End(slot.reader);}
    void Flush() {g_ctx11->Flush();}
    d2vr_shared_reader::Poll GetData() {
        const HRESULT hr=g_ctx11->GetData(slot.reader,nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH);
        return hr==S_OK ? d2vr_shared_reader::Poll::Ready :
            hr==S_FALSE ? d2vr_shared_reader::Poll::Pending : d2vr_shared_reader::Poll::Failed;
    }
    uint64_t Now() const {return GetTickCount64();}
    void PauseThread() {SwitchToThread();}
};
static bool CanReadSharedUpload(ID3D11ShaderResourceView* source) {
    for(auto& slot:g_sharedCapture)if(source && source==slot.srv11) {
        SharedReaderFenceOps ops{slot};return slot.fence.CanRead(ops);
    }
    return true;
}
static bool SealSharedUpload(ID3D11ShaderResourceView* source) {
    for(auto& slot:g_sharedCapture)if(source && source==slot.srv11) {
        SharedReaderFenceOps ops{slot};return slot.fence.SealRead(ops);
    }
    return true;
}
static bool WaitSharedReader(SharedCaptureSlot& slot) {
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_cpu_sync::Scope cpuReader(d2vr_cpu_sync::SharedReader);
#endif
    SharedReaderFenceOps ops{slot};
    if(slot.deferred) {
        // Caller must reclaim the immediate-context lease before polling.
        if(!slot.fence.pending || !slot.fence.Wait(true,ops))return false;
        slot.deferred.reset();return true;
    }
    return slot.fence.Wait(g_lastConsumerReaderFence,ops);
}
static void FreeSharedCaptureSlot(SharedCaptureSlot& slot) {
    if(slot.producer)slot.producer->Release();
    if(slot.reader)slot.reader->Release();
    if(slot.srv11)slot.srv11->Release();
    if(slot.tex11)slot.tex11->Release();
    if(slot.surface9)slot.surface9->Release();
    if(slot.tex9)slot.tex9->Release();
    slot=SharedCaptureSlot{};
}
static bool HasSharedReaderDependency(const SharedCaptureSlot& slot) {
    // Legacy mode does not track reads at draw time. Seal the prior context
    // work at teardown/resize just as it is sealed before an overwrite.
    return slot.deferred || slot.fence.pending ||
        (!g_lastConsumerReaderFence && slot.reader && slot.srv11);
}
static bool ReleaseSharedCaptureSlot(SharedCaptureSlot& slot) {
    if(g_stereoTransferQueue)g_stereoTransferQueue->Drain();
    if(HasSharedReaderDependency(slot) && !WaitSharedReader(slot)) {
        g_stereoTransferFault=true;return false;
    }
    FreeSharedCaptureSlot(slot);return true;
}
static bool ReleaseSharedCapture() {
    if(g_stereoTransferQueue)g_stereoTransferQueue->Drain();
    // Verify BOTH readers before freeing either lifetime. A failed event keeps
    // every resource and each incomplete packet alive and refuses Reset.
    for(auto& slot:g_sharedCapture)
        if(HasSharedReaderDependency(slot) && !WaitSharedReader(slot)) {
            g_stereoTransferFault=true;
            Log("[StereoTransferWorker] eye reader incomplete; retain both capture slots and refuse Reset/reuse");
            return false;
        }
    for(auto& slot:g_sharedCapture)FreeSharedCaptureSlot(slot);
    g_stereoTransferFault=false;g_splitCaptureUnavailable=false;return true;
}

static bool EnsureSharedCapture(IDirect3DDevice9* dev,unsigned capture=0) {
    if(capture>=2)return false;
    auto& slot=g_sharedCapture[capture];
    if(g_stereoTransferFault) return false;
    if (!g_zeroCopyEnabled) return false;
    if (!dev || !g_dev11 || g_zeroCopyFailures > 2) return false;

    // shared texture must match the backbuffer size/format
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &bb)) || !bb) return false;
    D3DSURFACE_DESC bd{};
    const HRESULT description=bb->GetDesc(&bd);
    bb->Release();
    if(FAILED(description))return false;
#ifdef D2VR_MATCHED_CAPTURE
    ObserveProtectedSourceSize(bd.Width,bd.Height,unsigned(bd.Format));
#endif
    if (slot.surface9 && slot.srv11) {
        if (bd.Width != slot.width || bd.Height != slot.height) {
            if (!ReleaseSharedCaptureSlot(slot)) return false;
        } else if (slot.producer && slot.reader) return true;
    }

    if (!slot.tex9) {
        HANDLE shared = nullptr;
        HRESULT hr = dev->CreateTexture(bd.Width, bd.Height, 1, D3DUSAGE_RENDERTARGET,
                                        D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &slot.tex9, &shared);
        if (FAILED(hr) || !slot.tex9) {
            if(!capture)++g_zeroCopyFailures;
            Log("[ZeroCopy] CreateTexture(shared slot=%u) failed hr=0x%08lX",capture,hr);
            return false;
        }
        slot.handle = shared;
        if(FAILED(slot.tex9->GetSurfaceLevel(0, &slot.surface9)) || !slot.surface9) {
            if(!capture)++g_zeroCopyFailures;
            ReleaseSharedCaptureSlot(slot);return false;
        }
        slot.width = bd.Width;
        slot.height = bd.Height;
    }
    if (!slot.tex11) {
        HRESULT hr = g_dev11->OpenSharedResource(slot.handle, __uuidof(ID3D11Texture2D),
                                                 reinterpret_cast<void**>(&slot.tex11));
        if (FAILED(hr) || !slot.tex11) {
            if(!capture)++g_zeroCopyFailures;
            Log("[ZeroCopy] OpenSharedResource(slot=%u) failed hr=0x%08lX",capture,hr);
            return false;
        }
        if (FAILED(g_dev11->CreateShaderResourceView(slot.tex11, nullptr, &slot.srv11))) {
            if(!capture)++g_zeroCopyFailures;
            ReleaseSharedCaptureSlot(slot);return false;
        }
        Log("[ZeroCopy] shared capture texture ready %ux%u (D3D9Ex -> D3D11)", slot.width, slot.height);
    }
    D3D11_QUERY_DESC q{D3D11_QUERY_EVENT, 0};
    if (!slot.reader && FAILED(g_dev11->CreateQuery(&q, &slot.reader))) return false;
    if (!slot.producer && FAILED(dev->CreateQuery(D3DQUERYTYPE_EVENT, &slot.producer))) return false;
    return slot.surface9 && slot.srv11 && slot.producer && slot.reader;
}

static bool PrepareSplitSharedCapture(IDirect3DDevice9* dev) {
    if(g_splitCaptureUnavailable)return false;
    if(EnsureSharedCapture(dev,1))return true;
    // The optional extra allocation cannot disable an otherwise working slot
    // zero or cause repeated allocation attempts on every frame. Reset retries.
    g_splitCaptureUnavailable=true;
    Log("[SharedCapture] secondary slot unavailable; retain single-slot fenced route until Reset");
    return false;
}

static bool CaptureBackbufferToShared(IDirect3DDevice9* dev,bool deferProducer=false,unsigned capture=0) {
    if(capture>=2)return false;
    auto& slot=g_sharedCapture[capture];
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_cpu_sync::Scope cpuCapture(d2vr_cpu_sync::SharedCapture);
#endif
    if (g_stereoTransferFault || !slot.surface9 || !slot.producer || !WaitSharedReader(slot)) return false;
    IDirect3DSurface9* src = nullptr;
    // Darkness II renders offscreen; check active RenderTarget0 first
    if (FAILED(dev->GetRenderTarget(0, &src)) || !src) {
        if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &src)) || !src) return false;
    }
#ifdef D2VR_MATCHED_CAPTURE
    MatchedCaptureInput(src,false);
#endif
    HRESULT hr = dev->StretchRect(src, nullptr, slot.surface9, nullptr, D3DTEXF_NONE);
    src->Release();
    if (FAILED(hr)) return false;

    // GPU fence: D3D9Ex and D3D11 run on separate queues; flush D3D9 batch before D3D11 reads
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_cpu_sync::Scope cpuProducer(d2vr_cpu_sync::D3D9Fence);
#endif
    if (FAILED(slot.producer->Issue(D3DISSUE_END))) return false;
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_replay::d2vr_command_history::Scope command(dev,::d2vr_command_history::ProducerFlush,
        d2vr_replay::g_replayEye,0,capture,D3DGETDATA_FLUSH);
#endif
    if(deferProducer) {
        // Submit on the original thread; worker only polls this issued query.
        hr=slot.producer->GetData(nullptr,0,D3DGETDATA_FLUSH);
#ifdef D2VR_MATCHED_CAPTURE
        command.Finish(hr);
#endif
        g_srcW=slot.width; g_srcH=slot.height;
        return hr==S_OK || hr==S_FALSE;
    }
    const ULONGLONG start = GetTickCount64();
    hr=slot.producer->GetData(nullptr,0,D3DGETDATA_FLUSH);
    while (hr == S_FALSE) {
        if (GetTickCount64() - start > 2000) return false;
        SwitchToThread();
        hr=slot.producer->GetData(nullptr,0,0);
    }
#ifdef D2VR_MATCHED_CAPTURE
    command.Finish(hr);
#endif
    g_srcW = slot.width; g_srcH = slot.height;
    return hr == S_OK;
}

