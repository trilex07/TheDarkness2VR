# Handoff: larger full-packet worker

**Date**: 2026-11-03
**Task ID**: engine-parallel-work-20261103
**Role**: MAIN
**Branch**: codex/engine-parallel-work-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: parent ec5d6875; installed accepted DLL B97B4477, cinematic-theater-20261102T193328001
**Integration commit**: pending acceptance; no merge or deployment

## Goal and status

User requests substantial actual offload, preserving resolution and accepted
stereo/hands/tentacles. Full VS packet worker implemented and offline validated,
default OFF. Benefit remains unproven and the realistic-sized synthetic CPU
work is only~0.19ms. Do not describe correct threading as a large FPS win.

## Context loaded

BASELINE, workflow, failed approaches, performance plan/audit/candidate and
native submission/ownership evidence. Details and all gates:
`docs/research/full-packet-worker-20261103.md`.

## Changes and evidence

New pure packet core, bounded owned worker and actual replay integration;
independent startup switch FullPacketPrepareWorker. Tests compile the existing
serial implementation verbatim as oracle.942908/942909 packet/lifecycle checks PASS;
production+diagnostic mixed consumer tests each2.3million records PASS;
protected-native build, head,47/130 hands and five-mode installer PASS.
Private build/packet-*.log. Exact engine job-pool and resource-class findings
in research doc; native exports remain TEMP, not Git. No new native patch.

## Next steps

1. Frozen and verified package recorded below; no install performed.
2. MAIN may deploy only in an authorized test window with game closed. No
   launch/install was requested in this turn. Keep accepted Steam files intact.
3. Use Packets versus Off with identical DLL/settings to measure total costs
   and hits/fallback/join. Headset correctness and combat FPS are NOT RUN.
4. The CPU ceiling is small; if runtime is neutral/negative, retain OFF rather
   than treating thread count as progress. Larger native work still needs a
   proven producer/lifetime/dependency boundary, not a guessed GraphicsWorker
   setting or an unprotected D3D9 device migration.

## Notes

Never stage build/ or test-output/ wholesale. Old shader-cache package remains
frozen and separate. Installed DLL/INI/protected receipt hashes must still match
cinematic-theater baseline; Git branch does not identify deployed binary.

## Frozen candidate receipt

- Implementation7aa8bed1; normalized source73abfa4a38e261b88fd381b6ed4b8f240ab79b01.
- Package `C:/Users/Trilex/Darkness2VR/.main-camera-shake/build/full-packet-worker-20261103`.
- Protected-native x86 `/O2` DLL SHA256
  `160C8F78E8CA51EE524E74569F0478C7F523E688CF8D1978CE0B33FD6A5955B4`.
- Frozen default Packets: ShaderDemandCache0 /ReplayPrepareWorker0 /
  FullPacketPrepareWorker1. Off sets all three0; identical DLL.
- VerifyOnly PASS, all five profiles independently checked against baseline:
  no INI differences beyond the three switches. All installed receipt hashes
  still match. No install, launch, cache modification or physical acceptance.
- Package Install.ps1 restores exact DLL/INI with -Restore after an authorized
  closed-game install. Do not use build.ps1 -Install as the rollback package.
- User explicitly approved remote push after the initial automatic-review
  rejection. Push through6ae49498 succeeded to `trilex07/TheDarkness2VR`,
  branch`codex/engine-parallel-work-20261103`. No merge or installation.
- User challenged the small CPU ceiling. MAIN acknowledged that selecting a
  safe ownership boundary did not establish a worthwhile optimization target;
  the full-packet timing should have stopped further expansion of this path.
  Keep default OFF and do not recommend headset testing as a major FPS fix.

## Follow-up: expensive-target search

See `docs/research/combat-critical-path-search-20261103.md`. Existing complete
combat ETW was correlated with1528 PresentMon intervals. Slow/fast quartile gap
4.412ms is distributed: native engine+1.217, d3d9+1.086 nominal leaf exposure;
PhysXCore changes only0.024. Historical instrumented DLL differs from installed.
4813 native PCs queried read-only against exact EXE;476 unknowns preserved.
Six growing native functions traced into geometry/material/buffer submission,
not independent multi-ms math. Native00943F50 task is already on15 workers,
3053 exclusive sample weights and zero main-thread leaf samples.009AA4D0
dispatches them through indirect CALL at009AA571. No substantial new offload
candidate proven; do not implement speculative native/D3D9 thread migration.
New reusable `tools/perf/compare_sampled_frame_costs.py`, three parser tests PASS.
Private exports stay TEMP, analysis JSON stays test-output. Steam DLL rehash
still B97B4477; no install/launch or runtime code changes. Next target is an
entire redundant renderer submission phase with ownership/state proof, not
another tiny worker. Detailed reproducibility/limits/hashes are in the report.
