# Native LODScale trial

**Date**: 2026-11-01  
**Task ID**: gaze-lod-candidate-20261101  
**Role**: MAIN  
**Branch**: codex/gaze-nis-main-integration  
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR  
**Baseline**: installed stable DLL E6F49A7E9E520D24502B9F5ABAFB4E0DAE6A9D7DDC0ACDE86486DAD4128DB1BD, stock EXE D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1  
**Status**: In Progress

## Goal

Make a reversible headset candidate to test whether native engine LOD improves CPU/FPS.

## Evidence and change

- Stock selector `0x00AE319E` has exact `F3 0F 59 05 74 E0 10 01`, multiplying distance by `Graphics.LODScale` at `0x0110E074`.
- Stock EnableLOD dispatch `0x008513A3` has exact `80 3D 54 DE 10 01 00`.
- `0x0110E074` is in the stock image's writable non-executable `.data`. Runtime checks both instruction sequences, image type and section flags before changing the data float.
- `EngineLODScale=1.25` applies during active 3D gameplay and restores the original scale outside it. Default `0` leaves the engine untouched. This is a whole-scene LOD trial, not gaze/per-object LOD.
- `tools/deploy-engine-lod-trial.ps1` verifies stock EXE, current DLL and profile, backs up DLL, four INIs and four caches, then installs the candidate and one game INI setting. It is reversible through the package rollback.

## Verification and next step

- Protected-native x86 `/O2` build and profile gate passed. Headset quality and CPU/FPS effect are unproven.
- After installation, user checks the same problem scene, both eyes, nearby models and FPS; inspect `[EngineLOD]` logs for `active=1`, effective selector mode and reapplications.
- If visual quality regresses or no native selector effect is present, restore the complete package. Do not merge to main on offline evidence alone.
