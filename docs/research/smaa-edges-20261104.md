# Edge and hand shimmer: SMAA trial

User redirected effort from possibly native reflection motion to edge/hand
shimmer. Neither reflection nor shadow acceptance is implied by this change.
The retained12-second Steam video is3840x1080,712 frames, approximately59.5fps
encoding. The excerpt350..365 retains the hand silhouette while its black
outline shows stair steps. This supports testing spatial aliasing separately
from intermittent missing meshes; it cannot rule out missing meshes elsewhere.

SMAA upstream is iryoku/smaa, commit71c806a838bdd7d517df19192a20f0c61b3ca29d,
MIT. Unmodified algorithm and lookup tables are vendored with license. The
wrapper uses1x HIGH/luma detection, display-referred UNORM throughout, after
NIS. Gamma-space neighborhood blending is intentional to match the existing
bridge; no global color conversion. No fake jitter, depth or motion vectors.
This is spatial antialiasing, not temporal reconstruction or a claimed complete
solution to shader/specular shimmer.

Three passes per eye: clear/detect edges, clear/calculate weights, neighborhood
blend. Scratch is cleared between eyes. Completed independent input textures
are retained. One deferred command list is recorded for each input pair and
reused against current pixels. ExecuteCommandList(TRUE) restores immediate
context state. Changed input identity rebuilds; invalid/mixed inputs return no
output pair. Native/current eyes remain the caller's fallback. Reset releases
retained resources. No CPU image readback or GPU waits in the runtime path.
SMAA is OFF by default; optional Diagnostics/SmaaExperiment preserves main INI.

RTX5070 hardware fixture PASS: exact constant colors/alpha, independent eyes,
same-input new pixels (not stale commands),155 changed diagonal-edge pixels,
changes confined within3.1pixels of the synthetic boundary, preserved viewport/
topology/render target, input-identity rebuild, mixed-size/null/feedback rejection
and reset recovery.3232x3300 BOTH-eye checkerboard fixture40 iterations measured
GPU1.253ms/pair, CPU enqueue0.001ms/pair. These are isolated fixture costs, not
recovered game FPS or physical SteamVR timing. Readback is confined to fixture.

The actual recorded hand frame356 was processed through this same hardware
path; private before/after crop shows reduced stair steps with retained skin
details. Video compression and one still frame prevent a temporal/HMD claim.
Private artifacts: test-output/aliasing-audit-20261104 and build/smaa-eye-test.log.
Runtime simulator validation and headset acceptance must be recorded separately.
