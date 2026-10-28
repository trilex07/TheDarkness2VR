#pragma once
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <array>
#include <cstring>
#include <memory>
#include "render_command_worker.h"

// Only the mod-owned interop copy is asynchronous. Engine resource updates,
// replay, original Present, XR calls and all API HRESULTs stay on their caller.
// Main must lend exclusive use of its D3D11 immediate context until Finish.
namespace d2vr_transfer {
template<class T> class Ref {
    T* value_ = nullptr;
public:
    Ref() = default;
    explicit Ref(T* value) : value_(value) { if(value_) value_->AddRef(); }
    Ref(const Ref& other) : Ref(other.value_) {}
    Ref(Ref&& other) noexcept : value_(other.value_) { other.value_=nullptr; }
    Ref& operator=(Ref other) noexcept { std::swap(value_,other.value_); return *this; }
    ~Ref() { if(value_) value_->Release(); }
    T* Get() const { return value_; }
    T* operator->() const { return value_; }
};

struct Job {
    Ref<IDirect3DQuery9> producer;
    Ref<ID3D11DeviceContext> context;
    Ref<ID3D11Query> reader;
    Ref<ID3D11RenderTargetView> target;
    Ref<ID3D11Buffer> constants;
    Ref<ID3D11VertexShader> vs;
    Ref<ID3D11PixelShader> ps;
    Ref<ID3D11RasterizerState> raster;
    Ref<ID3D11SamplerState> sampler;
    Ref<ID3D11ShaderResourceView> source, depth;
    std::array<unsigned char,256> bytes{};
    UINT size=0,width=0,height=0;
    // Written by worker; read only after the queue completion barrier.
    HRESULT result=E_PENDING;
    bool readerSubmitted=false, readerComplete=false;
    // Prototype only: caller must retain this job and wait for this exact
    // reader query before overwriting the shared input or tearing it down.
    // Finish then returns the immediate-context lease, not GPU completion.
    bool deferReader=false;
    DWORD thread=0;
    LONGLONG start=0, producerDone=0, drawDone=0, end=0;

    static LONGLONG Clock() { LARGE_INTEGER value{}; QueryPerformanceCounter(&value); return value.QuadPart; }
    void Run() {
        thread=GetCurrentThreadId(); start=Clock();
        result=E_INVALIDARG;
        if(!producer.Get() || !context.Get() || !reader.Get() || !target.Get() ||
           !constants.Get() || !vs.Get() || !ps.Get() || !source.Get() ||
           !size || size>bytes.size() || !width || !height) { end=Clock(); return; }
        // Main has issued and FLUSHed this exact producer query before handoff.
        // Poll without FLUSH so unrelated right-eye replay is not forced out.
        const ULONGLONG producerStart=GetTickCount64();
        while((result=producer->GetData(nullptr,0,0))==S_FALSE) {
            if(GetTickCount64()-producerStart>2000) { result=HRESULT_FROM_WIN32(ERROR_TIMEOUT); end=Clock(); return; }
            SwitchToThread();
        }
        producerDone=Clock();
        if(result!=S_OK) { end=Clock(); return; }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        result=context->Map(constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        if(FAILED(result)) { end=Clock(); return; }
        std::memcpy(mapped.pData,bytes.data(),size);
        context->Unmap(constants.Get(),0);
        auto* rtv=target.Get(); auto* cb=constants.Get(); auto* smp=sampler.Get();
        context->OMSetRenderTargets(1,&rtv,nullptr);
        D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};
        context->RSSetViewports(1,&viewport); context->RSSetState(raster.Get());
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(),nullptr,0); context->PSSetShader(ps.Get(),nullptr,0);
        context->PSSetConstantBuffers(0,1,&cb); context->PSSetSamplers(0,1,&smp);
        ID3D11ShaderResourceView* sources[2]={source.Get(),depth.Get()};
        context->PSSetShaderResources(0,2,sources);
        context->Draw(3,0); readerSubmitted=true;
        ID3D11ShaderResourceView* nullSources[2]={}; ID3D11RenderTargetView* nullTarget=nullptr;
        context->PSSetShaderResources(0,2,nullSources);
        context->OMSetRenderTargets(1,&nullTarget,nullptr);
        context->End(reader.Get()); context->Flush();
        drawDone=Clock();
        if(deferReader) {result=S_OK;end=Clock();return;}
        const ULONGLONG readerStart=GetTickCount64();
        while((result=context->GetData(reader.Get(),nullptr,0,0))==S_FALSE) {
            if(GetTickCount64()-readerStart>2000) { result=HRESULT_FROM_WIN32(ERROR_TIMEOUT); end=Clock(); return; }
            SwitchToThread();
        }
        readerComplete=result==S_OK; end=Clock();
    }
};
struct Backend {
    bool Execute(const std::shared_ptr<Job>& job) { job->Run(); return true; }
};
using Queue=d2vr_render_worker::Queue<Backend,std::shared_ptr<Job>,2>;

// Stack-bound lease: normal, error and early-return paths join before the
// caller resumes D3D11 / overwrites shared input / destroys its resources.
class Pending {
    Queue* queue_=nullptr;
    std::uint64_t ticket_=0;
    std::shared_ptr<Job> job_;
    bool* fault_=nullptr;
public:
    Pending()=default;
    Pending(const Pending&)=delete;
    Pending& operator=(const Pending&)=delete;
    ~Pending() { Finish(); }
    bool Start(Queue& queue,std::shared_ptr<Job> job,bool& fault) {
        if(queue_ || fault) return false;
        const auto submitted=queue.Submit(job);
        if(!submitted.accepted) return false;
        queue_=&queue; ticket_=submitted.ticket; job_=std::move(job); fault_=&fault;
        return true;
    }
    bool Active() const { return queue_!=nullptr; }
    bool Finish() {
        if(!queue_) return true;
        const auto completed=queue_->Wait(ticket_);
        const bool ok=completed.complete && !completed.firstFailure &&
            job_->result==S_OK && (job_->readerComplete ||
                (job_->deferReader && job_->readerSubmitted));
        if(!ok) *fault_=true; // retain overwrite prohibition until resource teardown
        queue_=nullptr; job_.reset(); fault_=nullptr; return ok;
    }
};
}
