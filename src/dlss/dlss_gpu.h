// src/dlss/dlss_gpu.h - D3D11 guide textures for DLSS 4.5 / FSR:
//   * reversed depth: R32_FLOAT, 1 / (1 + z) from the scene depth SRV, or 0.5 when no depth is
//     handed in (a constant valid plane so NGX never sees NaNs or uninitialised memory);
//   * motion vectors: R16G16_FLOAT, (previous UV - current UV) from the per-eye camera rotation,
//     optional translation/depth, or world view-projection matrices;
//   * current-color bias mask: R8_UNORM, 1 where the reprojected previous colour disagrees with
//     the current 3x3 neighbourhood;
//   * object motion (cs_objpre + cs_objsearch + ps_objfix): 8x8 tile + 3x3 patch block-matching
//     for moving objects, hands, tentacles, and camera turns not represented in the pose.
//
// Zero mod dependencies so tools/dlss-host-tests.cpp can exercise it directly.
#pragma once
#include <stdint.h>
#include <d3d11.h>

namespace d2vr::dlss {

struct Mat3 { float m[3][3]; };

struct GuideParams {
    uint32_t w = 0, h = 0;
    ID3D11ShaderResourceView* sceneDepth = nullptr;   // optional; alpha or red channel = scene depth
    uint32_t depthW = 0, depthH = 0;
    Mat3     prevFromCur = {{{1,0,0},{0,1,0},{0,0,1}}};
    float    translation[3] = {0, 0, 0};              // in previous camera axes, engine units
    float    depthScale = 0.0f;                       // engine units per depth unit (0 = rotation-only)
    float    tanH = 1.0f, tanV = 1.0f;
    float    prevTanH = 1.0f, prevTanV = 1.0f;
    float    bodyDepth = 0.0f;                        // depth units; nearer texels are player body/arms: rotation-only
    bool     useVp = false;                           // use vpCur/vpPrev + camDelta instead of prevFromCur
    float    vpCur[16] = {};                          // camera-relative view-projection (row-vector)
    float    vpPrev[16] = {};
    float    camDelta[3] = {};                        // current minus previous camera in world units
    float    jitter[2] = {};                          // this image's projection jitter offset, render pixels
    float    prevJitter[2] = {};                      // the previous image's, when jitterKnown is true
    bool     jitterKnown = false;
    bool     historyValid = false;                    // false -> motion is (0, 0) everywhere
    ID3D11ShaderResourceView* preFg = nullptr;        // optional: scene depth before the foreground (hands/weapon) pass
};

struct AuditBin { double vecErr = 0, zeroErr = 0, masked = 0; uint64_t n = 0; };
const int kAuditBins = 8;
inline const char* const kAuditBinNames[kAuditBins] = {
    "<0.1", "0.1-0.3", "0.3-1", "1-2", "2-10", "10-50", "50-1000", "sky"};

struct FlowStats {
    uint64_t n = 0, rejected = 0, big = 0;
    double   ex = 0, ey = 0, eabs = 0;
    double   mt[2] = {}, mm[2] = {};
    double   pabs = 0, tabs = 0;
    uint64_t armsN = 0; double armsAbs = 0;
    uint64_t frames = 0; double frameShift = 0, frameScatter = 0;
    uint64_t jitFrames = 0; double jitShiftRaw = 0, jitExpect = 0, jitDot = 0, jitNorm = 0;
    double   bandMt[kAuditBins] = {}, bandMm[kAuditBins] = {}, bandErr[kAuditBins] = {};
    uint64_t bandN[kAuditBins] = {};
};

class GuideGpu {
public:
    GuideGpu() = default;
    ~GuideGpu() { shutdown(); }
    GuideGpu(const GuideGpu&) = delete;
    GuideGpu& operator=(const GuideGpu&) = delete;

    bool init(ID3D11Device* dev, char* why, size_t whyCap);
    bool run(ID3D11Device* dev, ID3D11DeviceContext* ctx, const GuideParams& p, char* why, size_t whyCap);

    bool mask(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
              bool historyValid, float lo, float hi, char* why, size_t whyCap, bool colourPart = true,
              ID3D11ShaderResourceView* sceneDepth = nullptr, ID3D11ShaderResourceView* preFg = nullptr);
    bool keep(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* color);
    void forget(int eye);
    void audit(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
               float expX = 0.0f, float expY = 0.0f);

    struct ObjParams {
        float ratio = 0.5f;
        float minGain = 0.02f;
        float minContrast = 0.06f;
        float minDeviation = 1.5f;
        float prior = 0.002f;
        bool  temporal = true;
    };
    struct ObjStats { uint64_t images = 0, tiles = 0, searched = 0, overridden = 0; };
    ObjStats objStats;
    bool objmotion(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
                   float jitShiftX, float jitShiftY, const ObjParams& op, char* why, size_t whyCap);

    ID3D11Texture2D* depth()  const { return depth_; }
    ID3D11Texture2D* motion() const { return motion_; }
    ID3D11Texture2D* bias()   const { return bias_; }
    uint64_t         bytes()  const;
    void             shutdown();

    AuditBin  bins[kAuditBins];
    FlowStats flow;
    float     bodyDepth = 0.3f;

private:
    bool ensure(ID3D11Device* dev, uint32_t w, uint32_t h, char* why, size_t whyCap);

    ID3D11VertexShader*      vs_       = nullptr;
    ID3D11PixelShader*       ps_       = nullptr;
    ID3D11PixelShader*       psMask_   = nullptr;
    ID3D11PixelShader*       psAudit_  = nullptr;
    ID3D11PixelShader*       psFlow_   = nullptr;
    ID3D11ComputeShader*     csObjPre_  = nullptr;
    ID3D11ComputeShader*     csObjTile_ = nullptr;
    ID3D11PixelShader*       psObjFix_ = nullptr;
    ID3D11Buffer*            cbObj_    = nullptr;
    ID3D11Buffer*            cb_       = nullptr;
    ID3D11RasterizerState*   raster_   = nullptr;
    ID3D11BlendState*        blend_    = nullptr;
    ID3D11DepthStencilState* ds_       = nullptr;
    ID3D11SamplerState*      linear_   = nullptr;
    ID3D11Texture2D*         depth_    = nullptr;
    ID3D11Texture2D*         motion_   = nullptr;
    ID3D11Texture2D*         bias_     = nullptr;
    ID3D11RenderTargetView*  depthRtv_  = nullptr;
    ID3D11RenderTargetView*  motionRtv_ = nullptr;
    ID3D11RenderTargetView*  biasRtv_   = nullptr;
    ID3D11ShaderResourceView* depthSrv_  = nullptr;
    ID3D11ShaderResourceView* motionSrv_ = nullptr;
    ID3D11ShaderResourceView* biasSrv_   = nullptr;
    ID3D11Texture2D*          prev_[2]    = {};
    ID3D11ShaderResourceView* prevSrv_[2] = {};
    bool                      prevOk_[2]  = {};
    uint64_t                  prevBytes_  = 0;
    ID3D11Texture2D*          auditTex_   = nullptr;
    ID3D11RenderTargetView*   auditRtv_   = nullptr;
    ID3D11Texture2D*          auditStage_[2] = {};
    ID3D11Texture2D*          flowTex_[2] = {};
    ID3D11RenderTargetView*   flowRtv_[2] = {};
    ID3D11Texture2D*          flowStage_[2][2] = {};
    float                     auditExp_[2][2] = {};
    bool                      auditPending_[2] = {};
    int                       auditNext_  = 0;
    ID3D11Texture2D*          tiles_     = nullptr;
    ID3D11UnorderedAccessView* tilesUav_ = nullptr;
    ID3D11Buffer*             list_      = nullptr;
    ID3D11UnorderedAccessView* listUav_  = nullptr;
    ID3D11ShaderResourceView* listSrv_   = nullptr;
    ID3D11Buffer*             listArgs_  = nullptr;
    ID3D11Buffer*             stats_     = nullptr;
    ID3D11UnorderedAccessView* statsUav_ = nullptr;
    ID3D11Buffer*             statStage_[3] = {};
    bool                      statPending_[3] = {};
    int                       statNext_  = 0;
    ID3D11ShaderResourceView* tilesSrv_  = nullptr;
    ID3D11Texture2D*          tilePrev_[2] = {};
    ID3D11ShaderResourceView* tilePrevSrv_[2] = {};
    bool                      tilePrevOk_[2] = {};
    ID3D11Texture2D*          motion2_   = nullptr;
    ID3D11RenderTargetView*   motion2Rtv_ = nullptr;
    uint32_t                  tilesW_ = 0, tilesH_ = 0, obj2W_ = 0, obj2H_ = 0;
    bool ensure_obj(ID3D11Device* dev);
    uint32_t w_ = 0, h_ = 0;
    bool     ready_ = false;
};

} // namespace d2vr::dlss
