#pragma once
// Diagnostic outcome of the current frame only. Never grants admission.
enum class NativeCullingReason : unsigned {
    Applied=0, NoBoundary=1, Context=2, Camera=3, Projection=4,
    PreviousProjection=5, ShaderProjection=6, Geometry=7, Rebuild=8
};
static inline const char* NativeCullingReasonName(NativeCullingReason reason) {
    switch(reason) {
    case NativeCullingReason::Applied:return "applied";
    case NativeCullingReason::NoBoundary:return "selected-boundary-not-reached";
    case NativeCullingReason::Context:return "frame-or-gameplay-context";
    case NativeCullingReason::Camera:return "selected-camera-contract";
    case NativeCullingReason::Projection:return "native-projection-contract";
    case NativeCullingReason::PreviousProjection:return "preceding-frame-projection-proof";
    case NativeCullingReason::ShaderProjection:return "live-shader-projection-match";
    case NativeCullingReason::Geometry:return "binocular-geometry-preparation";
    case NativeCullingReason::Rebuild:return "native-frustum-rebuild";
    }
    return "unknown";
}
