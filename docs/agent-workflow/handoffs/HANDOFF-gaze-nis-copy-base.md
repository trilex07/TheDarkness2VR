# Handoff: gaze NIS copy base

**Date**: 2026-11-01
**Task ID**: gaze-nis-copy-base
**Role**: MAIN
**Agent**: /root
**Branch**: codex/hud-head-tentacles
**Worktree**: current
**Baseline**: accepted source 4a26aabd; installed owner diagnostic AC6D3568.
**Integration commit**: f7c6a400

## Goal
Reduce postprocess work when valid gaze exists, using the latest installed
weapon-wheel/eye-tracking/frame-cap source. Preserve the native scene and
full-quality fallback when gaze is absent.

## Status
Candidate installed; user runtime measurement pending.

## Context Loaded
- [x] AGENTS.md, BASELINE.md, FAILED-APPROACHES.md, previous gaze LOD handoff.
- [x] Installed candidate source and latest log.

## Changes
- New opt-in `Render/FoveatedCopyBase`: copy completed eye texture, apply
  NIS only to outer and inner gaze rectangles. The center retains NIS
  sharpness; outside retains original image pixels. Invalid gaze uses the
  existing uniform NIS fallback. Requires same-size, single-sample image.
- New WARP test checks identical pixels outside the gaze rectangle and
  changed pixels within it.
- Deployment profile enables FoveatedCopyBase and turns off the obsolete
  read-only GazeLodDiagnostic logging. Nine-target rollback preserved.
- Isolated integration under build/gaze-lod-diagnostic-final/src; archived
  exact source and binary in the deployment package.

## Evidence
- Previous user log: NIS GPU pair roughly 0.28–0.30 ms in stable windows,
  with occasional higher samples. This is filter time, not whole frame.
- D3D11 WARP: outsideDiff=0, insideDiff=48 at 256x256 test pattern.
- Protected-native x86 DLL build PASS; profile gate PASS.
- Candidate DLL SHA256: 916B9A5151CC25F694A0B967862F3CEFFCC21CE23E8C6D857BF654B164DB7FDF.
- Installed 02:17; package C:/Users/Trilex/AppData/Local/Darkness2VR/packages/gaze-nis-copy-base-20261101T021726069.
  Independent verification: DLL hash matched, stock EXE matched, nine backup
  hashes matched, both active INIs set FoveatedCopyBase=1 and
  GazeLodDiagnostic=0, game closed. User launches manually.

## Next Steps
1. User runs same scene; compare `[NIS] GPU pair dispatch` samples with prior
   roughly 0.29 ms and check copyBaseEyes increasing.
2. Keep or restore using matching rollback based on visual/FPS evidence.

## Blockers
Native engine CPU LOD selection remains unhooked. This candidate reduces
postprocess GPU work and proxy command submission only, with no claim of
engine CPU improvement or verified FPS gain yet.

## First runtime measurement (2026-11-01 ~02:23)
User launched game and wore headset. Bridge initially started late, then
reported flags=07 and 293–314 valid gaze samples per five seconds. The
copyBaseEyes counter grew by 240 over 120-frame windows with stable gaze,
proving both eyes used the fast path. NIS GPU pair samples in those windows
were about 0.129–0.133 ms, compared with roughly 0.28–0.30 ms in the prior
foveated three-pass runtime log. That is about 0.16 ms lower for this filter,
not a measured engine CPU or whole-frame FPS improvement. Invalid gaze used
the original uniform path at about 0.237–0.243 ms. Recent XRCadence was
89.6–90.2 world-bearing layers/s at 90 Hz but explicitly is not simulation
FPS; frame-boundary p95 was 16 ms. No NIS dispatch failure found. A separate
stale-camera refusal window occurred earlier during this long game run and
is not attributed to this change.
