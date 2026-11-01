# Handoff: current installed source on main

**Date**: 2026-11-01
**Task ID**: main-current-20261101
**Role**: MAIN
**Agent**: /root
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed DLL 916B9A51; previous retained rollback 3FDEB05C (BASELINE.md).
**Integration commit**: a2fdaf58, merge 316135c; main promotion pending.

## Goal
Promote all currently installed weapon-wheel, eye-tracking, HUD, frame-cap and
NIS source to Git main with source and rollback evidence. Continue research on
larger native engine and CPU savings.

## Status
Integrated and offline tested; main promotion pending.

## Context Loaded
- [x] AGENTS.md, WORKFLOW.md, BASELINE.md, FAILED-APPROACHES.md.
- [x] Installed candidate source and gaze NIS runtime log.

## Changes
- Based on main-weapon-wheels (descendant of main), copied exact installed
  source deltas from the protected candidate and enabled FoveatedCopyBase in
  versioned INI. Merged codex/hud-head-tentacles without conflicts.
- BASELINE.md records installed candidate, rollback and performance limit.

## Evidence
- Protected-native x86 build PASS from integrated source.
- Eye tracking 26/26; weapon wheel 137/137; wheel geometry 60,749,999
  checks; D3D11 WARP gaze NIS outsideDiff=0 insideDiff=48.
- Installed candidate: 916B9A5151CC25F694A0B967862F3CEFFCC21CE23E8C6D857BF654B164DB7FDF.
- Runtime gaze NIS GPU pair ~0.13 ms; prior three-pass ~0.28-0.30 ms.
- All nine protected rollback targets verified in installed package.

## Next Steps
1. Commit baseline and handoff, fast-forward local main and push main.
2. Review newest CPU frame timings; identify a native engine selector or
   rendering phase with measurable CPU benefit, then design narrow diagnostic.

## Blockers
Native gaze LOD owner and decoration-only classification are not proven.
Current NIS change does not reduce native engine CPU work.
