#pragma once
#include <d3d9.h>
#include <algorithm>
#include <cstdint>
#include "sampler_caps_cache.h"

namespace d2vr_sampling {
// Captured world VS position is row-vector WVP c0..c3. Exact PS bytecode
// CA08BDC4 declares DiffuseMap at s0 and samples it with the material UV.
// Do not extend this to normal/depth/lightmap or fullscreen samplers by guess.
inline bool IsWorldDiffuseMaterial(uint32_t vs, uint32_t ps) {
    return vs == 0x293F456Bu && ps == 0xCA08BDC4u;
}

using SamplerSetter = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD,
                                                  D3DSAMPLERSTATETYPE, DWORD);
class ScopedDiffuseFilter {
    IDirect3DDevice9* device_ = nullptr;
    SamplerSetter setter_ = nullptr;
    DWORD min_ = 0, max_ = 0;
public:
    ScopedDiffuseFilter() = default;
    ScopedDiffuseFilter(const ScopedDiffuseFilter&) = delete;
    ScopedDiffuseFilter& operator=(const ScopedDiffuseFilter&) = delete;
    bool Apply(IDirect3DDevice9* dev, SamplerSetter setter, unsigned requested) {
        if (device_ || !dev || !setter || requested <= 1) return false;
        IDirect3DBaseTexture9* base = nullptr;
        if (FAILED(dev->GetTexture(0, &base)) || !base) return false;
        D3DSURFACE_DESC desc{};
        const bool material = base->GetType() == D3DRTYPE_TEXTURE &&
            base->GetLevelCount() > 1 && SUCCEEDED(static_cast<IDirect3DTexture9*>(base)->GetLevelDesc(0, &desc)) &&
            !(desc.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) &&
            desc.Width >= 64 && desc.Height >= 64;
        base->Release();
        if (!material) return false;
        D3DCAPS9 caps{};
        if (FAILED(dev->GetDeviceCaps(&caps)) || caps.MaxAnisotropy <= 1 ||
            !(caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) ||
            FAILED(dev->GetSamplerState(0, D3DSAMP_MINFILTER, &min_)) || min_ != D3DTEXF_LINEAR ||
            FAILED(dev->GetSamplerState(0, D3DSAMP_MAXANISOTROPY, &max_))) return false;
        const DWORD quality = std::min(caps.MaxAnisotropy, std::max(max_, (DWORD)requested));
        if (FAILED(setter(dev, 0, D3DSAMP_MAXANISOTROPY, quality))) return false;
        if (FAILED(setter(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC))) {
            setter(dev, 0, D3DSAMP_MAXANISOTROPY, max_);
            return false;
        }
        device_ = dev; setter_ = setter;
        return true;
    }
    ~ScopedDiffuseFilter() {
        if (device_) {
            // Bypass the recording hook: replay gets the same scoped correction,
            // not extra sampler events left in the game's command stream.
            setter_(device_, 0, D3DSAMP_MINFILTER, min_);
            setter_(device_, 0, D3DSAMP_MAXANISOTROPY, max_);
        }
    }
};
} // namespace d2vr_sampling
