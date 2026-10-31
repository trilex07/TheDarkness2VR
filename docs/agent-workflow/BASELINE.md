# Baseline and current MAIN candidate

Recorded by MAIN on 2026-10-31. This file is a provenance snapshot, not a live
Steam check or permission to deploy. Do not assume Git main is installed.

## Accepted rollback baseline (retained, not currently installed)
- **Accepted DLL SHA256**: 3FDEB05CD94D6FC4E14C6C446A2859DA5F9A9E832AF5348D4602E4A7EF03C3EA
- **Accepted source commit**: 4a26aabdce15903eb5efe48bd707d47e200175e5
- **Accepted rollback**: %LOCALAPPDATA%/Darkness2VR/packages/nvidia-warmup-73a461b0-20261030/rollback

The retained rollback manifest verifies the DLL, all four INIs and four shader/
warmup caches. Restore the complete matching package, not only a DLL with an
incompatible newer manifest. Prior ConsumedRecordCopy validation preserved
stereo/hands/tracking. This does not certify newer experimental switches.

## Current installed MAIN candidate (provisional)

Updated 2026-10-31 after authorized four-branch deployment. Both x86 builds and
affected offline regressions PASS; this new combined candidate is NOT headset-accepted.
- **Current DLL SHA256**: BFF8B0A9F5BE83C511762E66C78C5903BD9FB3A48FD3D2F81BF15C035FBD1CFA
- **Current source commit**: 7159787160cf1702836ebd08de4594805421d460
- **Current Steam INI SHA256**: C252004214079D174E89D32A1D2F3EF985C109EB0B0FCAC76B103F2A2AF8E692
- **Current local INI SHA256**: 6083B9E7A3BEE21464A884FD73AD13657A3C89AB2260CCEBDCEE41197E722049
- **Current rollback**: %LOCALAPPDATA%/Darkness2VR/packages/four-branch-performance-20261031/rollback
- **Receipt and matching symbols**: same package, deployment.json and candidate/
- **Runtime feedback**: intermittent right-eye artifacts during gameplay. Combined candidate NOT visually accepted; cause not established. Log/profile/cache preserved in four-branch-performance-20261031/runtime-20261031T155300 and quarantine-20261031T155425.
- **Current new switches**: OverlapNativePresent=0 and ReuseSharedCaptureColor=0 quarantined after feedback; PipelineVariantLearning=1 retained. DLL unchanged; no new launch or FPS claim.
- **Preserved**: Mode4, ConsumedRecordCopy=1, PipelineWarmup=1, DirectRecordCopy=0, ReplayBufferVersions=0, diagnostic NativeCameraCover=0. Game not launched.
- **Task handoff**: handoffs/HANDOFF-four-branch-performance.md

### Previous installed candidate (retained by current rollback)

- **Installed DLL SHA256**: 34641A8981444EA05723E6F5F3D669099F6FECD3ABC1142ED1C686C6E42EC232
- **Installed INI SHA256**: 764EEADF2E5FB21E4F1A95B3884F39B8E75D3E94765CC10F74C9BCE5D5B8DF0B
- **Installed local INI SHA256**: FE242875007DA0BFDCE05B99A15256A22256550609A934CB4BA2BE18E0D17020
- **Deployed DLL source commit**: f24ced92f01823f070e1a2b9361d72b920ed6b92
- **Verified source commit**: f24ced92f01823f070e1a2b9361d72b920ed6b92 (exact frozen diagnostic DLL/PDB/MAP build)
- **Integration branch**: codex/main-single-owner-d3d9-validation
- **Latest MAIN runtime evidence commit**: eab97b35
- **Acceptance status**: StereoTransferWorker=4 retained provisionally; not unconditional acceptance or stable 90 FPS
- **Previous rollback**: %LOCALAPPDATA%/Darkness2VR/packages/single-owner-d3d9-20261031/rollback

Current settings include ConsumedRecordCopy=1, StartupShaderPool=1,
PipelineWarmup=1, StereoTransferWorker=4, NativeViewBoundary=3,
NativeCameraCover=0, DirectRecordCopy=0, ReplayBufferVersions=0. Source defaults
are not deployed settings. Read the current handoff/evidence before changing any.

One paired authorized mode-1/mode-4 test is already complete. Same diagnostic
DLL, identical settings apart from mode, restored identical startup caches,
warmup 1377/1377, zero PresentMon losses, real device flags 0x54 vs 0x50.
Application interval mean 14.73 -> 13.73 ms; p95 18.09 -> 15.50 ms;
p99 19.49 -> 16.96 ms. Qualified user feedback: visuals appear correct and the
light collector no longer noticeably costs ~20 FPS. Both x86 builds and 19
regression suites passed. No objective headset latency measurement.

## Current unresolved performance work

A 58.98 ms worst interval remains; the enclosing D3D9 producer-fence window
reached 42.656 ms. Its exact driver dependency is unproven; do not identify it
with the earlier 15.7 ms NVIDIA event or blame transfer ownership without proof.
NativeSubmissionFilter/NativeBindingFilter fail closed equally in both runs
(enabled=0, denied=0x12, skipped=0). No speculative cache/admission bypass.

Evidence: [MAIN comparison](../main-single-owner-d3d9-validation.md),
[retained measurements](../validation/single-owner-d3d9-20261031.json).
Continue [the performance handoff](handoffs/HANDOFF-single-owner-d3d9.md).
Do not redo integration or repeat the completed paired capture by default.

## Update rule

MAIN updates this snapshot only from verified build/deployment identities and
explicitly qualified runtime evidence. Documentation commits are not DLL source
commits. SIDE may cite this file but never update accepted/deployed status,
install files, launch Steam or perform in-game tests. No build/install/runtime
smoke test is needed for documentation-only changes.
