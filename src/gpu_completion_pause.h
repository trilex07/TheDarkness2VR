#pragma once
#include <d3d11_4.h>
#include <windows.h>
#include <cstdint>
// Advisory wait only: an event wakeup NEVER authorizes resource reuse. The
// caller must still observe its original event query returning S_OK.
// Use only while owning the immediate-context lease, on one thread.
namespace d2vr_gpu_wait {
class CompletionPause {
    ID3D11DeviceContext4* context_=nullptr;
    ID3D11Fence* fence_=nullptr;
    HANDLE event_=nullptr;
    UINT64 ticket_=0;
    bool pending_=false;
    bool failed_=false;
public:
    CompletionPause()=default;
    CompletionPause(const CompletionPause&)=delete;
    CompletionPause& operator=(const CompletionPause&)=delete;
    ~CompletionPause(){
        // A faulted/hung device must not receive an asynchronously signaled
        // recycled HANDLE. Retain the registration on the exceptional timeout.
        if(!Drain())return;
        if(context_)context_->Release();
        if(fence_)fence_->Release();
        if(event_)CloseHandle(event_);
    }
    bool Prepare(ID3D11Device* device,ID3D11DeviceContext* context){
        if(failed_)return false;
        if(context_)return true;
        if(!device || !context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
        ID3D11Device5* device5=nullptr;
        HRESULT hr=device->QueryInterface(IID_PPV_ARGS(&device5));
        if(SUCCEEDED(hr))hr=context->QueryInterface(IID_PPV_ARGS(&context_));
        if(SUCCEEDED(hr))hr=device5->CreateFence(0,D3D11_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_));
        if(device5)device5->Release();
        if(SUCCEEDED(hr))event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(FAILED(hr) || !event_){failed_=true;return false;}
        return true;
    }
    bool Pause(DWORD maximumMs=4){
        if(failed_ || !context_ || !fence_ || !event_)return false;
        // Never replace an outstanding registration. An old signal may wake
        // early, but cannot make the original query complete or permit reuse.
        if(pending_){
            const auto complete=fence_->GetCompletedValue();
            if(complete==UINT64_MAX){failed_=true;return false;}
            if(complete>=ticket_){pending_=false;return true;}
        }
        if(!pending_){
            ResetEvent(event_);
            if(ticket_==UINT64_MAX){failed_=true;return false;}
            ++ticket_;
            if(FAILED(context_->Signal(fence_,ticket_)) ||
               FAILED(fence_->SetEventOnCompletion(ticket_,event_))){failed_=true;return false;}
            pending_=true;
            context_->Flush();
        }
        const DWORD result=WaitForSingleObject(event_,maximumMs);
        if(result==WAIT_FAILED){failed_=true;return false;}
        return result==WAIT_OBJECT_0;
    }
    bool Drain(){
        if(!pending_)return true;
        const ULONGLONG start=GetTickCount64();
        for(;;){
            const auto complete=fence_->GetCompletedValue();
            if(complete==UINT64_MAX || complete>=ticket_){pending_=false;return true;}
            if(GetTickCount64()-start>=2000)return false;
            if(WaitForSingleObject(event_,4)==WAIT_FAILED)return false;
        }
    }
};
}
