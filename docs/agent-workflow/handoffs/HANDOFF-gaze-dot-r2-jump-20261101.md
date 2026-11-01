# Handoff: Gaze dot and R2 jump

**Date**: 2026-11-01
**Task ID**: gaze-dot-r2-jump-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: accepted rollback is source `4a26aabd`; previous installed native CPU candidate was DLL `70F43D5D...`; current installed candidate is below.
**Integration commit**: `70f4e28e7fea44c97ff7a478cfb93b15d2318d04` plus this baseline receipt.

## Goal

Hide the visible gaze indicator and prevent R2 from jumping in active gameplay; install with rollback and promote to main.

## Status

Installed and offline verified; headset acceptance pending.

## Context Loaded

- [x] BASELINE.md
- [x] FAILED-APPROACHES.md checked
- [x] Active game INI and live `[GazeMenu]` log

## Changes

- `Darkness2VR.ini`: `GazeDot=0` default.
- `src/gaze_menu.h`, `src/eye_gaze.inl`, `src/weapon_wheel_input.inl`: separate pad-confirm context from gaze cursor gate; active-world Flash cursor leaves R2 as trigger.
- `test/eye_tracking_test.cpp`: regression for gameplay, pause and worldless menu contexts.
- `tools/deploy-gaze-dot-r2.ps1`: preflight and protected nine-target installation; game INI changed by a one-byte replacement.

## Evidence

- Active log showed `pause=0 flashCursor=1 world=1`, while old `GazeMenuActive()` mapped R2 to XInput A (jump).
- Eye tracking host test 29/29; dual weapon wheel host test 137/137; protected-native profile PASS.
- Installed DLL SHA256 `E6F49A7E9E520D24502B9F5ABAFB4E0DAE6A9D7DDC0ACDE86486DAD4128DB1BD`.
- Stock EXE SHA256 `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
- Active game INI has `GazeDot=0`; all nine rollback copies match their source hashes.
- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/gaze-dot-r2-20261101T031753160`.
- Source commit: `70f4e28e7fea44c97ff7a478cfb93b15d2318d04`.

## Next Steps

1. User tests in headset: gaze dot absent, R2 shoots without jumping in active gameplay, R2 still confirms pause menus.
2. If rejected, run package `rollback/Restore.ps1` with game closed and inspect the new log.

## Blockers

Headset behavior cannot be confirmed offline; user launches the game.
