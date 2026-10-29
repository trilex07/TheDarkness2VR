// Included inside d2vr_replay after the recorder and original device functions.
// Resource-interface data slots are intercepted; no executable instructions change.
// A replay buffer follows the same range writes as its live peer, in stream order.
// CPU payloads exist only from Unlock to replay/abandon; no reads from GPU memory.
static constexpr size_t kBufferPayloadLimit = 16u << 20;
static constexpr size_t kBufferReplicaLimit = 64u << 20;
static constexpr size_t kBufferObjectLimit = 8192;
using BufferQueryFn = HRESULT(STDMETHODCALLTYPE*)(IUnknown*,REFIID,void**);
using BufferLockFn = HRESULT(STDMETHODCALLTYPE*)(IUnknown*,UINT,UINT,void**,DWORD);
using BufferUnlockFn = HRESULT(STDMETHODCALLTYPE*)(IUnknown*);
struct BufferVersion final : IUnknown {
    LONG refs=1;
    BufferQueryFn query=nullptr;
    BufferLockFn lock=nullptr, replicaLock=nullptr;
    BufferUnlockFn unlock=nullptr, replicaUnlock=nullptr;
    IUnknown* replica = nullptr;
    BufferVersion* previous = nullptr;
    BufferVersion* next = nullptr;
    UINT size = 0, offset = 0, bytes = 0;
    DWORD flags = 0;
    void* mapped = nullptr;
    unsigned locks = 0;
    bool dynamic = false, valid = true, index = false, linked = false, budget = false;
    std::atomic<bool> unsupportedAlias{false};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=__uuidof(IUnknown))return E_NOINTERFACE;
        *out=static_cast<IUnknown*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ULONG(InterlockedIncrement(&refs));}
    ULONG STDMETHODCALLTYPE Release() override;
};
static SRWLOCK g_bufferRegistryLock = SRWLOCK_INIT;
static BufferVersion* g_bufferRegistry = nullptr;
static size_t g_bufferReplicaBytes = 0, g_bufferObjects = 0;
static size_t g_bufferFrameBytes = 0;
static bool g_bufferFrameOpen = false;
static bool g_bufferWritesApplied = false;
static std::atomic<bool> g_bufferUnsafeAlias{false};
static UINT g_bufferMaxStreams = 16;
static const GUID kReplayBufferTag = {0x5b72d8e1,0x40a9,0x47c2,{0x91,0x18,0x53,0xb2,0xf8,0x4e,0x62,0xa7}};
static HRESULT(STDMETHODCALLTYPE* g_createVB)(IDirect3DDevice9*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9**,HANDLE*) = nullptr;
static HRESULT(STDMETHODCALLTYPE* g_createIB)(IDirect3DDevice9*,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer9**,HANDLE*) = nullptr;
static const GUID kBufferVersionTag = {0x447458e9,0x7e39,0x497d,{0x8a,0xd4,0xe9,0x9a,0xce,0x2f,0x13,0x05}};
static HRESULT STDMETHODCALLTYPE BufferQuery(IUnknown*,REFIID,void**);
static HRESULT STDMETHODCALLTYPE BufferLock(IUnknown*,UINT,UINT,void**,DWORD);
static HRESULT STDMETHODCALLTYPE BufferUnlock(IUnknown*);
struct BufferBackend {void** table=nullptr;BufferQueryFn query=nullptr;BufferLockFn lock=nullptr;BufferUnlockFn unlock=nullptr;bool installed=false;};
static BufferBackend g_bufferBackends[64]{};
static size_t g_bufferBackendCount=0;
static BufferBackend BufferMethods(IUnknown* object,bool install=false) {
    const auto table=*reinterpret_cast<void***>(object);
    AcquireSRWLockExclusive(&g_bufferRegistryLock);
    for(size_t i=0;i<g_bufferBackendCount;++i)if(g_bufferBackends[i].table==table) {
        const auto result=g_bufferBackends[i];ReleaseSRWLockExclusive(&g_bufferRegistryLock);return result;
    }
    BufferBackend result{table,reinterpret_cast<BufferQueryFn>(table[0]),
        reinterpret_cast<BufferLockFn>(table[11]),reinterpret_cast<BufferUnlockFn>(table[12]),false};
    if(install && g_bufferBackendCount<64) {
        // Store originals before publishing callbacks. Keep every other runtime
        // vtable entry intact, including implementation-specific tail entries.
        auto& saved=g_bufferBackends[g_bufferBackendCount++];saved=result;
        const bool query=PatchVTable(object,0,reinterpret_cast<void*>(&BufferQuery))!=nullptr;
        const bool lock=PatchVTable(object,11,reinterpret_cast<void*>(&BufferLock))!=nullptr;
        const bool unlock=PatchVTable(object,12,reinterpret_cast<void*>(&BufferUnlock))!=nullptr;
        saved.installed=query && lock && unlock;result=saved;
    }
    ReleaseSRWLockExclusive(&g_bufferRegistryLock);return result;
}
static BufferVersion* BufferInfo(IUnknown* object) {
    if(!object)return nullptr;
    BufferVersion* version=nullptr;DWORD bytes=sizeof(version);
    auto resource=static_cast<IDirect3DResource9*>(object);
    if(FAILED(resource->GetPrivateData(kBufferVersionTag,&version,&bytes)) || bytes!=sizeof(version) || !version)return nullptr;
    // D3DSPD_IUNKNOWN returns an owned reference. The source resource keeps its
    // own reference for its lifetime; all callers already own the source.
    version->Release();return version;
}
static void DropBufferReplica(BufferVersion* version) {
    auto replica=version->replica;version->replica=nullptr;
    AcquireSRWLockExclusive(&g_bufferRegistryLock);
    if(version->budget) {g_bufferReplicaBytes-=version->size;version->budget=false;}
    ReleaseSRWLockExclusive(&g_bufferRegistryLock);
    // Never call the runtime while holding the registry lock: resource private
    // data destruction can call back here while the runtime owns its own lock.
    if(replica)replica->Release();
}
ULONG STDMETHODCALLTYPE BufferVersion::Release() {
    const auto remaining=ULONG(InterlockedDecrement(&refs));
    if(!remaining) {
        AcquireSRWLockExclusive(&g_bufferRegistryLock);
        if(linked) {
            if(previous)previous->next=next;else g_bufferRegistry=next;
            if(next)next->previous=previous;
        }
        --g_bufferObjects;
        ReleaseSRWLockExclusive(&g_bufferRegistryLock);
        DropBufferReplica(this);delete this;
    }
    return remaining;
}
static HRESULT STDMETHODCALLTYPE BufferQuery(IUnknown* object,REFIID iid,void** out) {
    auto version=BufferInfo(object);
    const auto query=version?version->query:BufferMethods(object).query;
    const HRESULT hr=query(object,iid,out);
    // Alternate interface addresses could bypass the original interface hooks.
    if(version && SUCCEEDED(hr) && out && *out && *out!=object) {
        version->unsupportedAlias.store(true,std::memory_order_release);
        g_bufferUnsafeAlias.store(true,std::memory_order_release);
    }
    return hr;
}
static bool UploadBufferRange(BufferVersion* version,UINT offset,UINT bytes,DWORD flags,const void* data) {
    if(!version || !version->valid || version->unsupportedAlias.load(std::memory_order_acquire) || !version->replica || !data || !bytes ||
       offset>version->size || bytes>version->size-offset)return false;
    // Keep failure sticky even if the driver raises an exception during replay.
    version->valid=false;
    void* mapped=nullptr;
    HRESULT hr=version->replicaLock(version->replica,offset,bytes,&mapped,flags);
    if(FAILED(hr)) {version->valid=false;return false;}
    if(mapped)std::memcpy(mapped,data,bytes);
    hr=version->replicaUnlock(version->replica);
    version->valid=mapped && SUCCEEDED(hr);
    return version->valid;
}
static HRESULT STDMETHODCALLTYPE BufferLock(IUnknown* object,UINT offset,UINT bytes,void** out,DWORD flags) {
    auto version=BufferInfo(object);
    if(!version) {
        const auto lock=BufferMethods(object).lock;
        RecLockGuard guard;if(g_bufferFrameOpen && !(flags&D3DLOCK_READONLY))g_recOverflow=true;
        return lock(object,offset,bytes,out,flags);
    }
    RecLockGuard guard;
    const HRESULT hr=version->lock(object,offset,bytes,out,flags);
    if(FAILED(hr))return hr;
    ++version->locks;
    if(version->locks!=1 || !out || !*out || offset>version->size || bytes>version->size-offset) {
        version->valid=false;if(g_bufferFrameOpen)g_recOverflow=true;return hr;
    }
    version->offset=offset;version->bytes=bytes?bytes:version->size-offset;
    version->flags=flags;version->mapped=*out;
    return hr;
}
static void AdvanceBufferWrites();
static HRESULT STDMETHODCALLTYPE BufferUnlock(IUnknown* object) {
    auto version=BufferInfo(object);
    if(!version) {
        const auto unlock=BufferMethods(object).unlock;
        return unlock(object);
    }
    RecLockGuard guard;
    const bool write=version->locks==1 && !(version->flags&D3DLOCK_READONLY);
    bool captured=false;
    size_t record=0;
    if(write) {
        // Static or shared resources cannot be journaled. Reject an overlapping
        // recording, including the interval between live completion and replay.
        if(!version->dynamic || !version->valid || !version->replica || !version->bytes) {
            if(g_bufferFrameOpen)g_recOverflow=true;
        } else if(g_bufferFrameOpen) {
            if(g_recActive && !g_recOverflow && version->bytes<=kBufferPayloadLimit-g_bufferFrameBytes &&
               g_rec.size()<400000 && g_recData.size()<=UINT32_MAX-version->bytes) {
                try {
                    const auto offset=uint32_t(g_recData.size());
                    g_recData.resize(g_recData.size()+version->bytes);
                    std::memcpy(g_recData.data()+offset,version->mapped,version->bytes);
                    record=g_rec.size();
                    RecEmitLocked(OP_BufferWrite,version->bytes,uint32_t(uintptr_t(object)),
                        version->offset,version->flags,offset);
                    captured=g_rec.size()==record+1;
                    if(captured)g_bufferFrameBytes+=version->bytes;
                } catch(...) {g_recOverflow=true;}
            }
            if(!captured) {
                // The pair cannot be published, but its final contents must still
                // become next frame's baseline. Drain once, then copy each later
                // range directly; no unbounded journal or poisoned next frame.
                g_recOverflow=true;AdvanceBufferWrites();
                if(!UploadBufferRange(version,version->offset,version->bytes,version->flags,version->mapped))version->valid=false;
            }
        } else if(!UploadBufferRange(version,version->offset,version->bytes,version->flags,version->mapped)) {
            version->valid=false;
        }
    }
    const HRESULT hr=version->unlock(object);
    if(FAILED(hr)) {
        // Never replay a write which the original resource rejected.
        version->valid=false;
        if(captured)g_rec[record].n=0;
        if(g_bufferFrameOpen)g_recOverflow=true;
    }
    if(version->locks)--version->locks;
    if(!version->locks)version->mapped=nullptr;
    return hr;
}
static void TrackBuffer(IDirect3DDevice9* dev,IUnknown* object,UINT size,DWORD usage,
                        DWORD format,D3DPOOL pool,bool index,bool shared) {
    auto version=new(std::nothrow) BufferVersion{};
    if(!version)return; // Unknown bindings are rejected below.
    version->size=size;version->index=index;version->dynamic=(usage&D3DUSAGE_DYNAMIC)!=0;
    version->valid=!shared;
    AcquireSRWLockExclusive(&g_bufferRegistryLock);
    if(g_bufferObjects>=kBufferObjectLimit) {
        ReleaseSRWLockExclusive(&g_bufferRegistryLock);delete version;return;
    }
    ++g_bufferObjects;
    if(version->dynamic && version->valid && size<=kBufferReplicaLimit-g_bufferReplicaBytes) {
        g_bufferReplicaBytes+=size;version->budget=true;
    }
    ReleaseSRWLockExclusive(&g_bufferRegistryLock);
    const auto methods=BufferMethods(object,true);
    version->query=methods.query;version->lock=methods.lock;version->unlock=methods.unlock;
    version->valid=version->valid && methods.installed;
    if(version->budget && version->valid) {
        HRESULT hr;
        if(index) {
            IDirect3DIndexBuffer9* replica=nullptr;
            hr=g_createIB(dev,size,usage,D3DFORMAT(format),pool,&replica,nullptr);version->replica=replica;
        } else {
            IDirect3DVertexBuffer9* replica=nullptr;
            hr=g_createVB(dev,size,usage,format,pool,&replica,nullptr);version->replica=replica;
        }
        if(FAILED(hr))version->replica=nullptr;
        if(version->replica) {
            const DWORD tag=0x52564231;
            auto resource=static_cast<IDirect3DResource9*>(version->replica);
            if(FAILED(resource->SetPrivateData(kReplayBufferTag,&tag,sizeof(tag),0)))DropBufferReplica(version);
            else {
                const auto replicaMethods=BufferMethods(version->replica);
                version->replicaLock=replicaMethods.lock;version->replicaUnlock=replicaMethods.unlock;
            }
        }
    }
    if(version->dynamic && !version->replica) {version->valid=false;DropBufferReplica(version);}
    AcquireSRWLockExclusive(&g_bufferRegistryLock);
    version->next=g_bufferRegistry;if(g_bufferRegistry)g_bufferRegistry->previous=version;
    g_bufferRegistry=version;version->linked=true;
    ReleaseSRWLockExclusive(&g_bufferRegistryLock);
    // COM private data owns the metadata, so runtime Release/Reset semantics and
    // the object's complete original vtable layout remain intact.
    static_cast<IDirect3DResource9*>(object)->SetPrivateData(kBufferVersionTag,
        static_cast<IUnknown*>(version),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
    version->Release(); // failure leaves an untracked, rejected resource
}

static HRESULT STDMETHODCALLTYPE Hook_CreateVB(IDirect3DDevice9* dev,UINT size,DWORD usage,DWORD fvf,
                                               D3DPOOL pool,IDirect3DVertexBuffer9** out,HANDLE* shared) {
    const HRESULT hr=g_createVB(dev,size,usage,fvf,pool,out,shared);
    if(SUCCEEDED(hr) && out && *out)TrackBuffer(dev,*out,size,usage,fvf,pool,false,shared!=nullptr);
    return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_CreateIB(IDirect3DDevice9* dev,UINT size,DWORD usage,D3DFORMAT format,
                                               D3DPOOL pool,IDirect3DIndexBuffer9** out,HANDLE* shared) {
    const HRESULT hr=g_createIB(dev,size,usage,format,pool,out,shared);
    if(SUCCEEDED(hr) && out && *out)TrackBuffer(dev,*out,size,usage,DWORD(format),pool,true,shared!=nullptr);
    return hr;
}
static bool ReplayBufferWrite(const Rec& record,const uint8_t* data) {
    auto version=BufferInfo(reinterpret_cast<IUnknown*>(uintptr_t(record.a[0])));
    return record.n && uint64_t(record.a[3])+record.n<=g_recData.size() &&
        UploadBufferRange(version,record.a[1],record.n,record.a[2],data+record.a[3]);
}
static IUnknown* ReplayBuffer(IUnknown* object,bool& complete) {
    if(!object)return nullptr;
    auto version=BufferInfo(object);
    if(!version) {
        DWORD tag=0,bytes=sizeof(tag);
        if(SUCCEEDED(static_cast<IDirect3DResource9*>(object)->GetPrivateData(kReplayBufferTag,&tag,&bytes)) &&
           bytes==sizeof(tag) && tag==0x52564231)return object;
        complete=false;return nullptr;
    }
    if(!version->valid || version->unsupportedAlias.load(std::memory_order_acquire) || version->locks || (version->dynamic && !version->replica)) {
        complete=false;return nullptr;
    }
    return version->dynamic?version->replica:object;
}
static bool ReplayInheritedBuffers(IDirect3DDevice9* dev) {
    bool complete=true;
    // Stream limit cached at installation. No per-draw getters.
    for(UINT slot=0;slot<g_bufferMaxStreams;++slot) {
        IDirect3DVertexBuffer9* buffer=nullptr;UINT offset=0,stride=0;
        if(FAILED(dev->GetStreamSource(slot,&buffer,&offset,&stride))) {complete=false;continue;}
        if(buffer) {
            auto replay=static_cast<IDirect3DVertexBuffer9*>(ReplayBuffer(buffer,complete));
            if(replay!=buffer && FAILED(g_orig.SetStreamSource(dev,slot,replay,offset,stride)))complete=false;
            buffer->Release();
        }
    }
    IDirect3DIndexBuffer9* buffer=nullptr;
    if(FAILED(dev->GetIndices(&buffer)))complete=false;
    if(buffer) {
        auto replay=static_cast<IDirect3DIndexBuffer9*>(ReplayBuffer(buffer,complete));
        if(replay!=buffer && FAILED(g_orig.SetIndices(dev,replay)))complete=false;
        buffer->Release();
    }
    return complete;
}
static void AdvanceBufferWrites() {
    // Caller holds recorder lock. Drain at most once, including on overflow.
    if(!g_bufferWritesApplied)for(const auto& record:g_rec)if(record.op==OP_BufferWrite)
        ReplayBufferWrite(record,g_recData.data());
    g_bufferWritesApplied=true;
}
static void FinishBufferRecording() {
    AdvanceBufferWrites();
    g_bufferFrameOpen=false;g_bufferFrameBytes=0;
}
static void CloseBufferFrame() {
    RecLockGuard guard;g_recActive=false;FinishBufferRecording();
}
static void ResetBufferReplicas() {
    RecLockGuard guard;
    BufferVersion* pending[kBufferObjectLimit];size_t count=0;
    AcquireSRWLockExclusive(&g_bufferRegistryLock);
    for(auto version=g_bufferRegistry;version;version=version->next)if(version->dynamic) {
        version->AddRef();pending[count++]=version;
    }
    ReleaseSRWLockExclusive(&g_bufferRegistryLock);
    for(size_t i=0;i<count;++i) {
        pending[i]->valid=false;DropBufferReplica(pending[i]);pending[i]->Release();
    }
}
