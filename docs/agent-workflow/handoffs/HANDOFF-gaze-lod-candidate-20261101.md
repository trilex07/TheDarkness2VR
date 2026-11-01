# Native LODScale trial

**Date**: 2026-11-01  
**Task ID**: gaze-lod-candidate-20261101  
**Role**: MAIN  
**Branch**: codex/gaze-nis-main-integration  
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR  
**Baseline**: installed stable DLL E6F49A7E9E520D24502B9F5ABAFB4E0DAE6A9D7DDC0ACDE86486DAD4128DB1BD, stock EXE D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1  
**Status**: Installed; headset validation pending

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
- Source commit `97fd19cd`. Candidate installed at `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/engine-lod-trial-20261101T113328024`. Installed DLL SHA256 `9AFA35F2A4B3B74EFA79FBF2C10BCEA296F6A159126B407E1783B8E84F57A358`, stock EXE unchanged, active game INI has `EngineLODScale=1.25`, and all nine rollback copy hashes verified. Game was not launched by the agent.
- After installation, user checks the same problem scene, both eyes, nearby models and FPS; inspect `[EngineLOD]` logs for `active=1`, effective selector mode and reapplications.
- If visual quality regresses or no native selector effect is present, restore the complete package. Do not merge to main on offline evidence alone.

## First headset check and R2 follow-up

- Runtime log: native selector and writable scale verified; active 3D scene had `scale=1.250`, `enabled=1`, `mode=-1`, `lowest=0`, `reapplies=0`. User reported smoother play and correct image. This is subjective evidence, not measured CPU milliseconds or proof that gaze changes LOD.
- User also reported R2 causing a jump again. Prior `MenuConfirmActive` fix from `70f4e28e` remains in source. The menu gate is sampled every 100 ms; log shows pause gate transitions while the world is visible. A stale gate can route R2 to stock A after unpause. A fresh native pause check was added at input time, plus one `[R2Route]` log per trigger press to distinguish this path from an engine mapping.
- New x86 protected-native build and profile gate passed; `test/build_eye_tracking.ps1` passed 29/29. R2 behavior still requires a headset check. `tools/deploy-engine-lod-r2.ps1` backs up all nine targets before installing the updated DLL with `EngineLODScale=1.25` retained.
