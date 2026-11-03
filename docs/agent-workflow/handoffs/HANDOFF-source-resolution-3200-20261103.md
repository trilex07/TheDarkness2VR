# Handoff: native source resolution3200x3264

Date2026-11-03; MAIN; branch codex/source-resolution-3200-20261103.
Worktree C:/Users/Trilex/Darkness2VR/.main-camera-shake; parentc39e7124.
Current preimage D178DF2D DLL /5A5BFDE9 INI, exact AF-trial rollback,
FXAA0 and scoped AF0. User explicitly requests higher GAME render resolution.
Accepted E56BF027 functional reference is still recorded in BASELINE.

## Evidence and scope

Prior task baseline/FAILED-APPROACHES/workflow reviewed. Read current native
source sizing and stereo output code, historical clarity-depth and vr001
render findings, latest d3d9.log. Latest run outputRT0/depth2676x2728 confirmed;
half-sized INTZ1338x1364 also occurs, role unresolved. Half-size alone does NOT
prove the whole scene is half-res. Older matched3076x3140 captures found final
eye RGB equal to RT0, and full-resolution HDR Frame before composite; those
are historical, not current-frame internal-buffer inventory.

Critical finding: source_quality.h Choose refused all source targets over
8294400 pixels (including Exact).3200x3264=10444800 would fail ClientSize.Start
and leave native sizing unmodified despite RenderWidth/Height logging. Extend
ONLY explicit Exact input/output pixel bound to10444800; per-axis3840 and all
other guards remain. Native/Quality/High preserve their old8294400 limit.
No native offsets/EXE/IAT changes; same verified external USER32 trampoline
and native callsites create dependent targets as before.

Prepared profile changes only RenderWidth3200/RenderHeight3264, SourceQuality4
already installed. About+19.6% per dimension,+43.1% source pixels. Keep current
XR recommended2676x2728 output (MatchEyeResolutionToSource remains absent/0),
NisMode2, gaze-copy-base/center fallback, FXAA0 and scoped AF0. Existing eye
composite downsamples larger native source to recommended cache before NIS.
FOV/projection/stereo/hands/cinema/optimizations/assets unchanged. Actual game
allocation/viewport/internal-buffer sizes and perceived sharpness/FPS must be
verified after user launches; configuration request is NOT runtime proof.

## Offline validation

tools/frame-boundary-simulator/build-source-quality.ps1 PASS5 routes:
Quality, baseline Exact2676 immediate/deferred, Exact3200 immediate/deferred.
Actual hook fixture validates window/caller binding, native+resource callers,
one forward/LastError, denied game-image patches,1002 expected size requests,
boundaries and+1-height overflow refusal. Exact can accept later3200 native
input; automatic modes still reject it. Invalid3840square still denied.
Protected-native build and frozen-package receipt follow.

## Package / rollback

tools/prepare-source-resolution-candidate.ps1 freezes full DLL/PDB/MAP, exact
current DLL/INI backup and existing protected target hashes. Combined requests
3200x3264; Off uses2676x2728 on new DLL. Install.ps1 -Restore returns byte-exact
D178DF2D DLL /5A5BFDE9 INI. No launch or headset acceptance yet.

## Installed for requested trial

Source6d696052, protected-native build PASS. Frozen package VerifyOnly PASS
then closed-game ModeCombined installation PASS. Independent installed and
protected hashes PASS; INI comparison confirms only RenderWidth/RenderHeight
changed. DLL C16947091B10B09FE31492ECDE57B2E7513A2AA0D7E5835830A38DFAFFA2AC19,
INI0F846993C35EB47535A7862E7A2B0E44B199D07EBEC9BF3DC11C626310CF5024.
No game launch. New native allocation, viewport/internal pass inventory,
clarity and FPS still NOT RUN; inspect next user-run log before claiming
3200x3264 actually rendered. Previous log preserved by prior AF task.

## User rejection and exact rollback

2026-11-03 user reports lower FPS, CPU around25ms, hand flicker and reflections
following head; adds shadows have the same behavior. Preserved private runtime
log proves requested3200x3264 was allocated as actual RT0. This does not prove
the resolution change caused the lighting bug, which has older open evidence.
Closed-game package Restore returned D178DF2D/5A5BFDE9, then gaze-FXAA package
Restore returned accepted E56BF027/68EE5685 Combined+IsolatedEyeOutput.
Independent two-target and four-protected-target hashes PASS. No launch.
Continue in HANDOFF-render-artifacts-cinema-20261103.md; do not reinstall this
rejected trial automatically.
