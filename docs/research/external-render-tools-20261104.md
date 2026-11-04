# External tools for the active Darkness2VR renderer

Source-only review on 2026-11-04. User is testing installed candidate2119EA3C;
no deployment, API-layer registration, profiler attachment or game launch here.
Public shallow checkouts are ignored under tools-local. No third-party software
was executed. This review does not claim runtime compatibility.

## Pinned sources

- https://github.com/elliotttate/nsight-graphics-mcp
  f1d0c996bb20b880021ef0f391aec1057b8e1486
- https://github.com/elliotttate/OpenXR-CAS
  3a66b4cc77e4b509a762fdad3719ab0ba43382fd
- https://github.com/elliotttate/OFXR-Bridge
  f593f7456fde1d57be3f0791b828f911a6b6d2d6

Our process is x86, native renderer D3D9Ex, completed stereo/OpenXR device D3D11.
Having OpenXR does not remove the process architecture or graphics API gates.

## nsight-graphics-mcp: conditional diagnostic value

Python CLI/MCP wrapper plus SQLite capture indexing, capture diff and capability
inspection. It needs separately installed NVIDIA Nsight Graphics; it does not
provide a D3D9 interception implementation. Python version is not proof that
Nsight can attach to a particular x86 target. pyproject requires Python>=3.10.
README explicitly recommends ngfx_deep_capture_capability_report before choosing
serialization/replay paths; wrapper tool count is not target feature coverage.

NVIDIA's current feature matrix lists D3D12/Vulkan/OpenGL and a deprecated
D3D11 frame debugger; D3D9 is absent, and GPU Trace is not marked for D3D11:
https://docs.nvidia.com/nsight-graphics/UserGuide/appendix.html
Some older NVIDIA feature pages still advertise more D3D11 support. Verify the
EXACT installed tool/version/API/target architecture before any capture claim.

Useful if a compatible tool can inspect our final D3D11 passes, or a separate
supported host/test fixture. It cannot directly expose every original D3D9
floor/shadow draw through the existing renderer. Do not switch the accepted
renderer to DXVK/D3D9On12 merely to obtain a profiler capture. For25ms CPU/waits,
system scheduling evidence remains necessary; a GPU shader view alone cannot
prove where CPU time went. No SDK installation or capture attempted.

## OpenXR-CAS: optional sharpening alternative, not AA

MIT project, D3D11 CAS and Levels. README foregrounds x64, but actual solution
and vcxproj include Release|Win32, suffixed32 DLL/manifest and Install-Layer32.
Thus do not dismiss it solely on README architecture; Win32 build/runtime is
still NOT RUN. Defaults/build instructions mostly exercise x64.

layer.cpp dispatchCas copies an input region to pooled texture, dispatches
compute sharpening, then copies it back. xrReleaseSwapchainImage forwards the
release downstream and records its index; xrEndFrame later processes that
last-released texture. That ordering requires a swapchain ownership audit
before borrowing the layer wholesale. Serialization helpers are not evidence
of application ownership after release. Additional full-eye copies also matter
for our accepted isolated-eye-output path.

Best limited use: evaluate the CAS shader as an ALTERNATIVE to NIS sharpening
inside our owned completed-eye path, before image release. Not another global
layer stacked atop NIS/SMAA. It sharpens contrast, does not reconstruct missing
geometry, provide temporal AA, or resolve native shadows. Possible amplification
of high-frequency shimmer is a hypothesis to measure, not a measured regression.

## OFXR-Bridge: relevant idea, current x86 blocker

docs/BUILDING.md says64-bit Windows; docs/TROUBLESHOOTING.md explicitly says
32-bit games unsupported. CI builds x64; CMake links pinned
ffx_opticalflow_x64.lib and ffx_backend_dx12_x64.lib. A Win32 compiler switch
alone cannot produce a compatible current layer.

README and src/d3d11/d3d11_bridge.cpp implement a D3D11-to-D3D12 frame-generation
bridge, so our final graphics API conceptually fits even though D3D9 is native.
But the layer must load in the x86 process. An actual port/dependency rebuild or
an x86-to-x64 out-of-process image/pose/sync bridge is a separate substantial
project. No promise of a drop-in solution or successful x86 build.

Color-only optical flow, without native game depth/motion vectors; README notes
moving-object/disocclusion/head-rotation artifacts. Generated display frames do
not increase native simulation rate or eliminate a25ms CPU frame. Fast VR hands
and tentacles need explicit validation. Pacing, stereo/history, runtime image
ownership and extra memory/interop cost must pass before enabling. Repository
license is LGPLv3; source reuse/distribution requires separate license review.

## Decision

Do not disturb the active SMAA acceptance test. No immediate drop-in fix among
these three for our x86 CPU-bound renderer. First gate Nsight capabilities for
diagnostic use; consider CAS only as a controlled sharpening A/B after user
feedback. Keep OFXR as a distinct frame-generation research option with explicit
x86 architecture scope, not an unmeasured FPS optimization.
