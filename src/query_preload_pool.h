#pragma once
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <atomic>
#include <mutex>

namespace d2vr_query_preload {
constexpr unsigned MaxQueries=512;
using CreateFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DQUERYTYPE,IDirect3DQuery9**);
// Only never-issued objects are owned here. A transferred object is never
// revisited, retained, reset, recycled or supplied to another native owner.
class Pool {
    IDirect3DDevice9* device_;
    IDirect3DDevice9Ex* ex_=nullptr;
    HWND window_;
    uintptr_t caller_;
    CreateFn original_;
    unsigned limit_;
    std::mutex mutex_;
    std::array<IDirect3DQuery9*,MaxQueries> objects_{};
    std::array<HRESULT,MaxQueries> results_{};
    unsigned size_=0;
    bool usable_=false;
    bool Healthy() {
        return (ex_ ? ex_->CheckDeviceState(window_) : device_->TestCooperativeLevel())==D3D_OK;
    }
public:
    std::atomic<uint64_t> requests{0},hits{0};
    Pool(IDirect3DDevice9* device,HWND window,uintptr_t caller,CreateFn original,unsigned limit)
        :device_(device),window_(window),caller_(caller),original_(original),limit_(limit<=MaxQueries?limit:MaxQueries) {
        if(device_)device_->QueryInterface(__uuidof(IDirect3DDevice9Ex),reinterpret_cast<void**>(&ex_));
    }
    ~Pool(){BeginReset(device_);if(ex_)ex_->Release();}
    Pool(const Pool&)=delete;
    Pool& operator=(const Pool&)=delete;
    unsigned Pending(){std::lock_guard<std::mutex> lock(mutex_);return size_;}
    void BeginReset(IDirect3DDevice9* device) {
        if(device!=device_)return;
        std::array<IDirect3DQuery9*,MaxQueries> release{};unsigned count;
        {std::lock_guard<std::mutex> lock(mutex_);usable_=false;count=size_;size_=0;release.swap(objects_);}
        for(unsigned i=0;i<count;++i)if(release[i])release[i]->Release();
    }
    // Called synchronously on the device owner, before return to the native
    // engine at startup or after a successful Reset. Never a background refill.
    void EndReset(IDirect3DDevice9* device,HRESULT reset) {
        if(device!=device_ || !device_ || !original_)return;
        BeginReset(device);
        if(reset!=D3D_OK)return;
        if(!Healthy())return;
        const auto start=GetTickCount64();
        for(unsigned i=0;i<limit_;++i) {
            if(GetTickCount64()-start>=2000)break;
            IDirect3DQuery9* query=nullptr;
            const HRESULT hr=original_(device_,D3DQUERYTYPE_OCCLUSION,&query);
            if(FAILED(hr) || !query){if(query)query->Release();break;}
            std::lock_guard<std::mutex> lock(mutex_);
            objects_[size_]=query;results_[size_++]=hr;
        }
        {std::lock_guard<std::mutex> lock(mutex_);usable_=true;}
    }
    HRESULT Create(IDirect3DDevice9* device,D3DQUERYTYPE type,IDirect3DQuery9** out,uintptr_t caller) {
        if(device!=device_ || caller!=caller_ || type!=D3DQUERYTYPE_OCCLUSION || !out)
            return original_(device,type,out);
        ++requests;
        bool candidate;
        {std::lock_guard<std::mutex> lock(mutex_);candidate=usable_ && size_;}
        if(candidate && Healthy()) {
            std::lock_guard<std::mutex> lock(mutex_);
            if(usable_ && size_) {
                const auto index=--size_;*out=objects_[index];objects_[index]=nullptr;
                ++hits;return results_[index];
            }
        }
        return original_(device,type,out);
    }
};
}
