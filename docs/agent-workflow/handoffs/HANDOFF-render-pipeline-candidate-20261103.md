# Handoff: combined render-pipeline candidate

**Date**: 2026-11-03
**Task ID**: render-pipeline-candidate-20261103
**Role**: MAIN
**Branch**: codex/render-pipeline-candidate-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: deployed B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2,
receipt cinematic-theater-20261102T193328001; parent source9c42c450.
**Integration commit**: pending user headset acceptance, no main merge.

## Goal and status

User authorized fewer driver calls, broader parallel preparation and avoided
waits, then one candidate with prior optimizations. Implementation/offline
validation complete within proven ownership; package freezing follows commit.
Native whole object/material-list threading remains UNIMPLEMENTED: no safe
immutable producer boundary proven. Do not describe VS replay packets as that.
No runtime FPS gain or headset acceptance established.

## Context loaded

BASELINE, WORKFLOW, relevant FAILED-APPROACHES, performance-candidate and
present-overlap handoffs, combat-critical-path-search-20261103 evidence.
Detailed changes, limitations and checks:
docs/research/render-pipeline-candidate-20261103.md.

## Changes and evidence

Owner-epoch NativeMaterialFilter; raw sampler invalidation/restoration;
EarlyPacketPreparation before live-end capture/backup with UNKNOWN prefix,
parameter guard and lease; combined eight-profile frozen package tooling.
Prior ShaderDemandCache, FullPacketPrepareWorker and OverlapNativePresent
included; tiny ReplayPrepareWorker OFF. Accepted stereo/resolution unchanged.
Standard/protected builds PASS; native hooks10708, packet942915/942916,
real D3D9Ex sampler479, real interop2760 eye transfers including full resolution,
head/hands/tentacles, state/failure/order/lifecycle and installer checks PASS.
Legacy sampler CreateDevice blocked before test execution, retained as limitation.
Logs and retained binaries are private under test-output/ and build/.

## Next steps

1. Freeze build/render-pipeline-candidate-20261103 with DefaultMode Combined;
   record manifest source commit, DLL hash and VerifyOnly result below.
2. User has requested candidate preparation, not installation or launch in this
   turn. Installed B97 DLL/INI and all protected targets must remain identical.
3. Once a test window is explicitly opened, MAIN installs only with game closed;
   exact rollback is frozen Install.ps1 -Restore. No automatic launch or timer.
4. Controlled natural combat A/B is required before any gain/main acceptance.
   Larger native threading remains a separate RE task, not a completed step.
