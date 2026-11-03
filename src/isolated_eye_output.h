#pragma once
#include <d3d9.h>

// Owner-thread only. One persistent right-eye output; shared intermediates and
// depth remain on the established same-frame replay path. No worker D3D9 calls.
namespace d2vr_eye_output {
class Output {
    IDirect3DDevice9* owner_=nullptr; // identity; surfaces retain the device
    IDirect3DSurface9 *source_=nullptr,*right_=nullptr;
    IDirect3DBaseTexture9* sourceTexture_=nullptr;
    IDirect3DTexture9* rightTexture_=nullptr;
    bool valid_=false,active_=false;
public:
    Output()=default;
    Output(const Output&)=delete;
    Output& operator=(const Output&)=delete;
    ~Output(){Release();}
    void Release(){
        active_=false;valid_=false;
        if(right_)right_->Release();right_=nullptr;
        if(rightTexture_)rightTexture_->Release();rightTexture_=nullptr;
        if(sourceTexture_)sourceTexture_->Release();sourceTexture_=nullptr;
        if(source_)source_->Release();source_=nullptr;owner_=nullptr;
    }
    void Invalidate(){valid_=false;active_=false;}
    bool Prepare(IDirect3DDevice9* device,IDirect3DSurface9* source){
        active_=false;
        if(!device || !source)return false;
        if(owner_==device && source_==source && right_)return true;
        Release();
        D3DSURFACE_DESC d{};
        if(FAILED(source->GetDesc(&d)) || d.Pool!=D3DPOOL_DEFAULT ||
            !(d.Usage&D3DUSAGE_RENDERTARGET) || d.MultiSampleType!=D3DMULTISAMPLE_NONE ||
            (d.Format!=D3DFMT_X8R8G8B8 && d.Format!=D3DFMT_A8R8G8B8))return false;
        IDirect3DBaseTexture9* base=nullptr;
        const HRESULT container=source->GetContainer(__uuidof(IDirect3DBaseTexture9),reinterpret_cast<void**>(&base));
        if(SUCCEEDED(container) && base){
            if(base->GetType()!=D3DRTYPE_TEXTURE || base->GetLevelCount()!=1){base->Release();return false;}
            IDirect3DSurface9* level=nullptr;
            const HRESULT got=static_cast<IDirect3DTexture9*>(base)->GetSurfaceLevel(0,&level);
            const bool exact=SUCCEEDED(got) && level==source;
            if(level)level->Release();
            if(!exact){base->Release();return false;}
            sourceTexture_=base;
            if(FAILED(device->CreateTexture(d.Width,d.Height,1,D3DUSAGE_RENDERTARGET,d.Format,
                D3DPOOL_DEFAULT,&rightTexture_,nullptr)) || !rightTexture_ ||
                FAILED(rightTexture_->GetSurfaceLevel(0,&right_)) || !right_){Release();return false;}
        }else{
            if(base)base->Release();
            // Non-texture RTs/backbuffers return E_NOINTERFACE. Any other
            // failure is not proof of a standalone surface.
            if(container!=E_NOINTERFACE)return false;
            if(FAILED(device->CreateRenderTarget(d.Width,d.Height,d.Format,D3DMULTISAMPLE_NONE,
                0,FALSE,&right_,nullptr)) || !right_){Release();return false;}
        }
        source_=source;source_->AddRef();owner_=device;return true;
    }
    IDirect3DSurface9* Source()const{return source_;}
    IDirect3DSurface9* Right()const{return right_;}
    IDirect3DBaseTexture9* SourceTexture()const{return sourceTexture_;}
    bool Valid()const{return valid_;}
    bool Active()const{return active_;}
    // Initial/fallback seed only. Steady pairs preserve the right background
    // in place; no Save/Restore/backup copies are needed between pairs.
    bool Seed(IDirect3DDevice9* device,IDirect3DSurface9* pixels){
        if(device!=owner_ || !right_ || !pixels)return false;
        if(FAILED(device->StretchRect(pixels,nullptr,right_,nullptr,D3DTEXF_NONE))){Invalidate();return false;}
        valid_=true;return true;
    }
    void Activate(){active_=valid_ && right_;}
    void Deactivate(){active_=false;}
    void Commit(){valid_=right_!=nullptr;active_=false;}
    IDirect3DSurface9* Surface(IDirect3DSurface9* surface)const{
        return active_ && surface==source_?right_:surface;
    }
    IDirect3DBaseTexture9* Texture(IDirect3DBaseTexture9* texture)const{
        return active_ && sourceTexture_ && texture==sourceTexture_?rightTexture_:texture;
    }
    template<class Setter> bool RemapInheritedTextures(IDirect3DDevice9* device,Setter set){
        if(!active_ || !sourceTexture_)return true;
        D3DCAPS9 caps{};
        if(FAILED(device->GetDeviceCaps(&caps)))return false;
        for(unsigned i=0;i<21;++i){
            const unsigned slot=i<16?i:256+i-16;
            if(slot==256 && !(caps.DevCaps2&D3DDEVCAPS2_DMAPNPATCH))continue;
            if(slot>=257 && D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion)<3)continue;
            IDirect3DBaseTexture9* texture=nullptr;
            const HRESULT got=device->GetTexture(slot,&texture);
            if(FAILED(got)){if(texture)texture->Release();return false;}
            const auto mapped=Texture(texture);
            const HRESULT sent=mapped!=texture?set(device,slot,mapped):S_OK;
            if(texture)texture->Release();
            if(FAILED(sent))return false;
        }
        return true;
    }
};
}
