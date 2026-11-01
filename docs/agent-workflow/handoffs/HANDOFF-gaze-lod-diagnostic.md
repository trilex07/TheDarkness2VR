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
Installed; user runtime check pending.

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
