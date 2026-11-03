# Handoff: local FXAA before gaze NIS

**Date**: 2026-11-03
**Task ID**: gaze-fxaa-nis-20261103
**Role**: MAIN
**Branch**: codex/gaze-fxaa-nis-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: source81bddf2b, installed E56BF027 DLL / 68EE5685 INI,
user accepted "jest dobrze"; branch parent7c061cb3.
**Implementation commits**: f0ba1517,7edce2cf; no main merge requested.

## Goal / scope

User requests original (NIS0) periphery, local FXAA before gaze NIS. Explicit
answer: missing tracking uses a fixed image-center region. Preserve resolution,
accepted stereo/CPU optimizations/camera/hands/tentacles/cinema and native assets.
BASELINE, workflow, prior isolated-eye handoff and relevant FAILED-APPROACHES
entries read. No native executable patch or new offset.

## Implementation

- GazeFXAA=0 and FoveaCenterFallback=0 defaults preserve the old behavior.
  Prepared profile enables both and sets FoveaPeriphery=0 (transition0.5,
  fovea1.0). NisMode2 / FoveatedSharpen1 required. GazeFXAA forces copy-base,
  so outside the outer rectangle is ORIGINAL, not a NIS-strength-zero dispatch.
- src/gaze_fxaa.hlsl: compact independently written FXAA-style directional
  contrast/resampling filter, informed by NVIDIA FXAA whitepaper. This is NOT
  the vendor FXAA3.11 quality preset. Display-referred UNORM input, alpha intact,
  flat-area early-out and smooth mask from inner to outer gaze square.
  Shader embedded offline by tools/build-gaze-fxaa-shader.ps1, no runtime compiler.
- src/gaze_fxaa.inl: persistent independent per-eye SRV/UAV scratch, original
  copy supplies neighborhood outside the AA region, compute only over region.
  NIS reads AA scratch; final periphery copies original. All SRV/UAVs unbound
  after dispatch. Shutdown/resize release resources. Existing filter failure
  latch falls back until Reset. Adds two full-eye scratch allocations/copies;
  at2676x2728 additional scratch is about55.7MiB for the pair, not a CPU/FPS win.
- Missing tracking per eye selects image center (not a measured gaze ray).
  With tracking, existing asymmetric per-eye gaze projection remains in use.
- Rectangular NIS test found existing SDK-inclusive-bound issue: NVSharpen
  uses dst>kOutputViewportWidth/Height. Host supplies exclusive extents; now
  convert to w-1/h-1 AFTER config generation in NisDispatchEyeRect, retaining
  normalization/scale math. Without fix non32-aligned rectangles wrote one
  extra row/column. Official SDK shader unchanged.

## Evidence / limits

Installed log: native outputRT0, depth, caches and NIS2676x2728; NIS output same
size. Stock FXAA bypass5104FEF7 active. This does not prove all earlier scene
passes have identical detail density, or explain all softness; no hidden lower
resolution asserted. User's no-NIS softness observation remains valid.
FXAA is spatial and may soften fine detail; visual/temporal effectiveness is
NOT RUN in HMD. No resolution/FOV/mip-bias/texture/asset changes.
Reference: https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf

Offline PASS:
- test/build_gaze_fxaa.ps1: actual production host+embedded shaders on D3D11
  WARP, 65,750,864 checks at258x259,517x293,2676x2728; AA changes pixels before
  NIS, immutable originals, alpha-preserving AA scratch, original periphery,
  different eye images, flat right eye, gaze at borders/moving, persistent
  resources and Reset. C4324 alignment-padding warning explicitly suppressed;
  otherwise W4/WX. Initial non32-aligned periphery FAIL drove boundary fix.
- test/build_nis_eye_host_warp.ps1: legacy NIS modes/strength/cache/resize/
  Map failure/failure latch1000/recovery/full-resolution PASS. Fixture had
  drifted from foveated host dependencies; supplied actual defaults/math include.
- protected-head regression117021 PASS. Standard and protected-native builds PASS.

## Candidate / next steps

Factory tools/prepare-gaze-fxaa-candidate.ps1 freezes new DLL/PDB/MAP, exact
accepted DLL/INI rollback and protected target hashes. Default ModeCombined
enables AA+center/periphery0; ModeOff disables ONLY AA (center/periphery0 stay).
Install.ps1 -Restore restores exact accepted DLL/INI. No resolution change.
Freeze only after tracked commit. No Steam installation or launch yet. Runtime
GPU cost, shimmer, softness, gaze loss/blink transitions and UI text require
user HMD trial. WARP timings are NOT hardware/game performance evidence.

## Frozen candidate receipt

build/gaze-fxaa-nis-20261103, source7edce2cf; protected-native x86/O2.
- DLL D178DF2D7362F24766B29747635FA2E84F6CE784E9E8CADC731302F8FD4570DE.
- candidate.ini680B77D2A28D8585A939EA30E3AC81B57642939BE84E877BC11D7BE5AE4C47C6.
- candidate.json1DB3E9835C553A4E2D9C565A04F572C94449BD9BEFE12D1BDE13B3F265B97A26.
- Frozen Install.ps1 -VerifyOnly PASS, game closed: all payloads, exact accepted
  DLL/INI backups and protected installed targets verified. Steam unchanged.
- A/B AA-only profile comparison required before deployment. No launch/install.
