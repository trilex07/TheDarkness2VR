#pragma once
#include "stereo_geometry.h"
#include "shader_register_history.h"
#include <algorithm>
#include <cstring>

// Experimental full-packet CPU preparation. All inputs are value snapshots;
// no game memory, COM, D3D calls, logging, or global projection writes here.
// The serial implementation remains the independent acceptance oracle.
namespace d2vr_packet_prepare {
struct Shader {
    uint32_t hash=0;
    int wvpBase=-1,stpBase=-1,reflectedWvp=-1,reflectedStp=-1,reflectedPreviousWvp=-1;
    int activeWvp=-1,activeStp=-1;
    bool reflected=false,analyzed=false,namedConstants=false;
};
struct Effects {unsigned shiftCount=0;bool publish=false;float px=0,py=0;bool uiDependent=false;};
struct Params : d2vr_stereo::EyeShiftParams {
    unsigned projectionFix=0,diagnosticSkipVs=0;
    int lightingPreset=0;
    bool scale2DContent=false,fixMotionVectorProjection=false;
    bool projectionSnapshot=true;
    float projectionPx=0,projectionPy=0,uiX=1,uiY=1;
    Effects* effects=nullptr; // Run-local output, never retained by a queued packet.
};
inline void ShiftMatrix(float* p,int n,const Params& sp,bool estimate) {
    const bool known=sp.projectionPx>0 && sp.projectionPy>0;
    if(estimate){sp.effects->publish=true;
        d2vr_stereo::EstimatePxPy(p,known,sp.projectionPx,sp.projectionPy,&sp.effects->px,&sp.effects->py);}
    d2vr_stereo::ApplyEyeShiftToMatrix(p,n,sp,known,sp.projectionPx,sp.projectionPy,
        sp.headGeometry?sp.headClip:nullptr);
}
static bool ApplyShiftToUpload(float* dst, const float* src, unsigned start, unsigned count,
                               Shader* vi, const Params& sp,
                               const d2vr_shader_registers::Reference* registers=nullptr,
                               bool* perspectiveUpload=nullptr) {
    if(perspectiveUpload)*perspectiveUpload=false;
    if (vi && sp.diagnosticSkipVs && vi->hash == sp.diagnosticSkipVs) return false;

    int wvpBase = -1, stpBase = -1;
    bool namedStp = false, identifiedWvp = false;
    if ((sp.projectionFix & 1) && vi && vi->reflected) {
        wvpBase = vi->reflectedWvp;
        stpBase = vi->reflectedStp;
        namedStp = stpBase >= 0;
        identifiedWvp = wvpBase >= 0;
    } else if (vi && vi->analyzed) {
        wvpBase = vi->wvpBase;
        stpBase = vi->stpBase;
        identifiedWvp = wvpBase >= 0;
    }
    bool fallbackWvp = false;
    if (wvpBase < 0 && (!vi || !vi->namedConstants) && start == 0 && count >= 4) {
        // Engine semantic: c0..c3 is the per-object WorldViewProjection. Nothing else is
        // guessed - c8..c11 of un-analyzed shaders regularly holds light parameters
        // (LightViewPosRadius/ShadowParams/light direction) and shifting those corrupts
        // lighting differently per eye (run_40332 trace: c11 held the sun direction).
        // The w-column gate is strict on purpose: blur kernels and packed parameter
        // blocks also land at c0 with a small but nonzero w column.
        const float* m = src;
        const float wlen = std::sqrt(m[3] * m[3] + m[7] * m[7] + m[11] * m[11]);
        if (wlen > 0.2f) {
            fallbackWvp = true;
            wvpBase = 0;
        }
    }

    // Orthographic UI: never gets an eye translation (it floats at screen depth), but the
    // FOV scale IS applied to sprite-style blocks so 2D content keeps the angular size and
    // position it has outside VR. Without it the menu text grows past the visible area and
    // screen-attached effects (the darkness tentacles) sit in the lens periphery once the
    // submission FOV is widened. Fullscreen passes use hardcoded quads or scale ~1
    // matrices and are left alone.
    if (!fallbackWvp && wvpBase >= 0 &&
        (int)start <= wvpBase && (int)(start + count) >= wvpBase + 4) {
        const float* wvp = src + (wvpBase - (int)start) * 4;
        const float perspective = wvp[3] * wvp[3] + wvp[7] * wvp[7] + wvp[11] * wvp[11];
        if (perspective < 1e-8f) {
            sp.effects->uiDependent=true;
            float uiX=sp.fovScaleX,uiY=sp.fovScaleY;
            uiX=sp.uiX;uiY=sp.uiY;
            const float lin = std::max(std::fabs(wvp[0]), std::max(std::fabs(wvp[1]),
                 std::max(std::fabs(wvp[4]), std::fabs(wvp[5]))));
            const bool sprite = sp.scale2DContent && lin > 0.0f && lin < 0.05f &&
                                (uiX != 1.0f || uiY != 1.0f);
            if (!sprite) return false;
            memcpy(dst, src, sizeof(float) * 4 * count);
            float* matrix = dst + (wvpBase - (int)start) * 4;
            for (int i = 0; i < 4; ++i) {
                matrix[i * 4 + 0] *= uiX;
                matrix[i * 4 + 1] *= uiY;
            }
            ++sp.effects->shiftCount;
            return true;
        }
    }

    // WVP reference values for the ScreenTextureProjection identity check.
    float wvpRef[16];
    bool haveWvpRef = false;
    if (wvpBase >= 0 && (int)start <= wvpBase && (int)(start + count) >= wvpBase + 4) {
        memcpy(wvpRef, src + (wvpBase - (int)start) * 4, sizeof(wvpRef));
        haveWvpRef = true;
    } else if(registers) {
        haveWvpRef=registers->valid;
        if(haveWvpRef)std::memcpy(wvpRef,registers->values,sizeof(wvpRef));
    }

    // Exact MotionBlur_16 VS: c0=current WVP, c4=PrevWorldProj; its output
    // subtracts their projected NDC positions. Leaving c4 native creates motion
    // from stereo/FOV changes alone. A shader hash AND reflected register proof
    // limit this experiment to the captured consumer, not arbitrary c4 blocks.
    // Geometric head tracking needs previous-frame headClip history separately.
    // A CTAB name alone is insufficient: if current WVP compensation was
    // deselected, transforming only the previous matrix would create motion.
    const int previousBase = vi ? d2vr_stereo::PreviousProjectionBase(vi->hash,
        wvpBase,vi->reflectedPreviousWvp,sp.fixMotionVectorProjection,sp.headGeometry) : -1;
    bool modified = false;
    for (int k = 0; k < (previousBase>=0 ? 3 : 2); ++k) {
        const int b = k==0 ? wvpBase : k==1 ? ((wvpBase==stpBase) ? -1 : stpBase) : previousBase;
        if (b < 0) continue;
        if ((int)start > b || (int)(start + count) < b + 4) continue;
        const float* orig = src + (b - (int)start) * 4;
        // These bytecodes are shared by the player and world objects. Apply
        // the ordinary world transform to every upload here. A draw-time
        // scope below can cancel it only after confirming the actual mesh.

        bool screenProjection = false;
        if (k == 1) {
            if (sp.lightingPreset == 7) continue;
            if (namedStp || (haveWvpRef && d2vr_math::IsScreenTextureProjection(orig, wvpRef))) {
                screenProjection = true;
            } else {
                // Not a screen projection: keep previous-frame/secondary projection-like
                // matrices shifting like a WVP, leave constant clip->UV transforms alone.
                const float wlen = std::sqrt(orig[3] * orig[3] + orig[7] * orig[7] +
                                             orig[11] * orig[11]);
                if (wlen <= 0.02f) continue;
            }
        }

        if (!modified) {
            memcpy(dst, src, sizeof(float) * 4 * count);
            modified = true;
        }
        float* matrix = dst + (b - (int)start) * 4;
        if (screenProjection) d2vr_math::UnbiasScreenTextureProjection(matrix);
        // Only positively identified WVP blocks may refine the projection estimate used
        // for the OpenXR submission FOV.
        Params matrixParams = sp;
        if (k == 1 && sp.lightingPreset == 8) {
            matrixParams.transX = matrixParams.transY = matrixParams.separation = 0.0f;
        }
        ShiftMatrix(matrix, 4, matrixParams, identifiedWvp && k == 0);
        if(perspectiveUpload && k==0) {
            const float perspective=orig[3]*orig[3]+orig[7]*orig[7]+orig[11]*orig[11];
            *perspectiveUpload=std::isfinite(perspective) && perspective>=1e-8f;
        }
        if (screenProjection) d2vr_math::RebiasScreenTextureProjection(matrix);
        ++sp.effects->shiftCount;
    }
    return modified;
}

// Complete identified partial WVP/STP from the current DEVICE bank. Both
// matrices intersect the original upload, so the union has no unknown gaps.
// A 68-row upload can gain at most three missing rows at each end. Keep extra
// stack storage on this uncommon path, rather than every native constant call.
static constexpr unsigned kVertexStereoUploadRows=76;
__declspec(noinline) static bool ApplyPartialProjectionUpload(float* dst,const float* src,
        unsigned& start,unsigned& count,Shader* vi,const Params& sp,
        const d2vr_shader_registers::Reference& reference,int base,int auxiliary,bool* perspectiveUpload) {
    unsigned first=start,end=start+count;
    for(int matrix:{base,auxiliary})if(matrix>=0) {
        first=(std::min)(first,unsigned(matrix));end=(std::max)(end,unsigned(matrix)+4);
    }
    if(end-first>kVertexStereoUploadRows)return false;
    float raw[kVertexStereoUploadRows*4];
    // Original payload and its overlapping matrices cover [first,end).
    memcpy(raw+(start-first)*4,src,size_t(count)*16);
    if(base>=0)memcpy(raw+(unsigned(base)-first)*4,reference.values,sizeof(reference.values));
    if(auxiliary>=0)memcpy(raw+(unsigned(auxiliary)-first)*4,reference.partialValues,sizeof(reference.partialValues));
    if(!ApplyShiftToUpload(dst,raw,first,end-first,vi,sp,&reference,perspectiveUpload))return false;
    start=first;count=end-first;return true;
}
static bool ApplyVertexStereoUpload(float* dst,const float* src,unsigned& start,unsigned& count,
        Shader* vi,const Params& sp,const d2vr_shader_registers::Reference* reference,
        bool* perspectiveUpload=nullptr) {
    const int base=vi?vi->activeWvp:-1;
    if(src && reference && count && count<=68 && start<=256 && count<=256-start) {
        const auto partial=[&](int matrix) {
            return matrix>=0 && matrix<=252 && start<unsigned(matrix)+4 && start+count>unsigned(matrix) &&
                !(start<=unsigned(matrix) && start+count>=unsigned(matrix)+4);
        };
        const int wvp=reference->valid && partial(base)?base:-1;
        const int auxiliary=reference->partialBase==(vi?vi->activeStp:-1) &&
            partial(reference->partialBase)?reference->partialBase:-1;
        if(wvp>=0 || auxiliary>=0)
            return ApplyPartialProjectionUpload(dst,src,start,count,vi,sp,*reference,wvp,auxiliary,perspectiveUpload);
    }
    return ApplyShiftToUpload(dst,src,start,count,vi,sp,reference,perspectiveUpload);
}


}
