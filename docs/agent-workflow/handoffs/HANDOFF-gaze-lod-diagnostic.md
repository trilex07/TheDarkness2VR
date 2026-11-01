# Handoff: gaze LOD configuration diagnostic

**Date**: 2026-11-01
**Task ID**: gaze-lod-diagnostic
**Role**: MAIN
**Agent**: /root
**Branch**: codex/hud-head-tentacles
**Worktree**: current
**Baseline**: accepted source 4a26aabd; current installed v11/frame-cap
integration archived in package below. See BASELINE.md for historical details.
**Integration commit**: pending for deployment script/documentation; diagnostic
include committed in 983ca7bb.

## Goal
Install a read-only native LOD/gaze diagnostic on the latest installed source,
preserving gaze, weapon wheels and existing frame cap. User launches the game.

## Status
Installed; user runtime test logged, object-level observation still pending.

## Context Loaded
- [x] AGENTS.md, BASELINE.md, FAILED-APPROACHES.md, relevant reverse engineering.
- [x] docs/research/gaze-lod-resume.md; private Ghidra evidence.
- [x] Exact installed source archive from engine-frame-cap-zero package.

## Changes
- src/gaze_lod_diagnostic.inl checks stock EXE read signatures and logs
  effective native LOD settings plus fresh gaze every five seconds.
- Isolated source integration in build/gaze-lod-diagnostic-final/d3d9.cpp;
  complete source retained in deployed package candidate directory.
- tools/deploy-gaze-lod-diagnostic.ps1 backs up nine protected files,
  verifies candidate and original EXE, inserts GazeLodDiagnostic=1 into
  both active Render profiles, verifies untouched bytes, and retains Restore.ps1.
- No object-level observation or quality change in this candidate.

## Evidence
- Installed DLL SHA256:
  148538F92CB37B2AF2D166282972B29FF1C23E46E7D70BDB1787D700719889CF.
- Pre-install DLL:
  7DB3D6475C8E78E25A793226559504A9B848EC8B7047DD0588E8571398D5FE38.
- Game EXE:
  D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
- Package:
  C:/Users/Trilex/AppData/Local/Darkness2VR/packages/gaze-lod-diagnostic-20261101T015029208.
  Full rollback in package/rollback/Restore.ps1.
- Protected x86 build PASS; profile gate protected-native; 26/26 eye
  tracking and 137/137 weapon wheel host tests PASS.
- Post-install independent verification: installed DLL hash, all nine backup
  hashes, both active INI keys PASS. Game not launched by agent.

## Next Steps
1. User launches game and checks that scene, gaze and wheel controls work.
2. Read `%LOCALAPPDATA%/Darkness2VR/d3d9.log` for `[GazeLOD]` lines.
3. Establish selected scene list ownership at native 5828B0/B2A0E0 and
   private resource type before object-level observations. Do not infer CPU
   savings from config-only logs.

## Blockers
None for configuration diagnostic. Object-level native list hook remains
unproven and is the next reverse-engineering gate.

## First user runtime test (2026-11-01 ~01:53)

Installed DLL still SHA256 148538F9...; game closed after user test.
`%LOCALAPPDATA%/Darkness2VR/d3d9.log` has 19 `[GazeLOD]` records.
In gameplay the effective settings were enable=1, lowest=0, scale=1.0000,
mode=-1, bias=0. Gaze became valid after bridge start; sampled direction
changed with look, e.g. (-0.084,-0.015,-0.996), later (0.080,-0.267,-0.960).
EyeTracking bridge reported 287/287 valid samples in one five-second window.
The first windows had no bridge section and gaze=0; this is expected fallback.
`FrameCap` logged native 60 -> 120 with mode0. No fatal log line found.
This proves config/gaze availability only, not selected per-object LOD or
CPU improvement. No automatic game launch or further deployment.

Read-only Ghidra follow-up: selector 8513A0 is called at 58290A inside
5828B0, which is in turn called at AEA455 inside AEA1A0. 5828B0 receives
the view-list on stack and uses its own renderer receiver for 8513A0;
it is distinct from the existing selected-player observer's scene reference.
Export: C:/Users/Trilex/darkness2-native-re-settings/gaze-lod-call-path-2.txt.

## Selected owner probe (2026-11-01)

Stock EXE Ghidra/Capstone evidence: 9922F0 is vtable FF8818 slot +94,
constructed at 56CD8B. It loads object+274 and calls 776DC0 at 99230C;
776DC0 later reaches AEA1A0 and the LOD selector. FF8818 slot +180
forwards to the same +274 (477540). The selected 985D80 camera path
owns a reference at its renderer+2A4 and calls its slot +180, making
that reference a candidate owner, but identity in live play is unverified.

An isolated protected-native build adds a throttled read-only probe inside
the admitted selected-player callback. It logs the candidate owner vtable,
its +274 renderer and renderer vtable. No LOD selection or quality changes.
Source: src/gaze_lod_owner_diagnostic.inl; isolated integration in
build/gaze-lod-diagnostic-final/src/d3d9.cpp and native_aim.inl.
Candidate AC6D3568... passed build and protected-profile preflight.
Full nine-target installer: tools/deploy-gaze-lod-owner.ps1.
Installed at 02:05 in game, user launches manually. Package:
C:/Users/Trilex/AppData/Local/Darkness2VR/packages/gaze-lod-owner-20261101T020523890.
Independent verification: installed AC6D3568AF0D670F69A8330D08A57B9F3EA4E602783F499A4BEC449848F47E1A,
9/9 rollback backups hashed, stock EXE unchanged, game closed. Awaiting
user runtime log for `selected-view owner` before any engine LOD mutation.
