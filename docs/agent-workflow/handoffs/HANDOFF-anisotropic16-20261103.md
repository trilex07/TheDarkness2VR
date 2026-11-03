# Handoff: existing scoped anisotropic16 trial

Date2026-11-03; MAIN; branch codex/anisotropic-16x-20261103.
Worktree C:/Users/Trilex/Darkness2VR/.main-camera-shake; parent29c1bf06.
Baseline: current D178DF2D DLL with FXAA-Off INI5A5BFDE9; accepted functional
reference remains isolated-eye E56BF027 as recorded in BASELINE.

User asks to try16x filtering after rejecting FXAA. Configuration-only: reuse
existing bytecode-matched WorldDiffuseAnisotropy path, no DLL/source algorithm
change. Current log confirms target16 but scoped flag0. Setting cap alone
does not promote linear minification. Enable WorldDiffuseAnisotropy1 and
explicit TextureAnisotropy16; FXAA0/NIS/center fallback/resolution retained.

Evidence loaded: BASELINE/workflow and FAILED-APPROACHES from immediately
preceding task; material_sampling.h, stereo_replay.inl WorldDiffuseScope and
docs/research/vr001-render-fixes-20261025.md narrow-sampling section.
Exact VS293F456B / PSCA08BDC4 diffuse sampler0 only, mipmapped ordinary2D
textures>=64px. Clamp to device support and restore after draw. No guessed
global sampler changes, mip bias or resolution changes. Scope coverage in
current user scene unproven. This does not provide geometric edge AA.

Factory tools/prepare-anisotropic16-candidate.ps1 freezes exact current DLL/
INI rollback and existing protected file hashes. Requires byte-identical DLL
to installed version. Combined enables scoped16x; Off disables scope while
keeping explicit target16; Restore returns byte-exact previous DLL/INI.

Standalone test first tried legacy HAL software-VP: device creation returned
0x8876086A before sampler assertions. This is NOT a filter test PASS. Existing
historical424-check HAL result is documented in the research file, not newly
measured here. D3D9Ex rerun result / frozen receipt / installation follows.
No game launch or physical shimmer/clarity acceptance yet.

## Installed configuration trial

Existing material fixture rerun with -Ex outside restricted GPU sandbox:
PASS479 checks, real D3D9Ex HAL sampler/rollback/stateblock, no Steam/XR.
Frozen build/anisotropic-16x-20261103 from b4ed0fa8; VerifyOnly PASS, then
closed-game ModeCombined install PASS. DLL remains byte-identical D178DF2D;
new INI4F781C01614947EBF01247897938BA0678FE3A36B1106622F974AE667172F772.
Independent hashes and profile comparison PASS: only AF settings changed.
GazeFXAA0, center fallback1, NisMode2 and2676x2728 retained. No launch.
Runtime material coverage, visual quality, shimmer and cost are NOT RUN.
Rollback: this frozen package Install.ps1 -Restore restores previous FXAA-Off
DLL/INI exactly (INI5A5BFDE9). Changes/receipt committed locally.

## Runtime FAIL and completed rollback

User: no visual benefit, FPS90->40. MAIN saved the runtime log privately at
test-output/anisotropic16-rejected-20261103/d3d9.log before further launches.
Later XRCadence windows show38.8-53 world-bearing submissions/s at physical90Hz;
GPUFrame spans and per-draw setter/getter costs do not isolate causality.
Game already closed: frozen Install.ps1 -Restore PASS; independently verified
every before/protected hash. Current DLL D178DF2D, INI5A5BFDE9 (exact previous
FXAA-Off state). No launch. FPS recovery after restart NOT RUN. Trial rejected,
added FAILED-APPROACHES; do not enable this scope again without new evidence.
