# R2 jump after death and reload

**Date**: 2026-11-01  
**Task ID**: r2-postload-20261101  
**Role**: MAIN  
**Branch**: codex/gaze-nis-main-integration  
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR  
**Baseline**: restored DLL `999047FBE28943C29FE5C06B601B657B5122DC0B88915FC4AC18B65BB17C24F5`, EngineLODScale=1.25 and previous NIS profile after rejected LOD 2.0/NIS trial.  
**Status**: In Progress

## Goal

Keep R2 as a trigger after death/load without losing R2 confirmation in actual pause menus.

## Root cause evidence

- In the rejected runtime, eight `R2Route` presses after a level reload logged `menu=1 world=1 inputContext=1`; the mod converted those presses to XInput A, and the user saw jumps.
- The existing `MenuConfirmActive` and window-thread gaze menu logic read `+0x198` and `+0xEC` from `RegionMgr`. The pinned native A4D340 predicate reads those fields from `GameRules`, reached through the verified `RegionMgr+0xBC` intrusive reference. See `src/native_menu_input_snapshot.h::ObservePause` and `docs/research/native-menu-input-contract-20261029.md`.
- The actual `NativePauseState` runtime receipt before the reload had `showing=0` through the GameRules graph while the direct RegionMgr gate later flipped. Thus the direct read was not a valid pause check.

## Change and verification

- `src/eye_gaze.inl`: use two equal, read-only `ObservePause` snapshots through the pinned graph for menu confirmation and gaze menu pause status. Unknown or changed objects fail closed to trigger fire.
- `src/weapon_wheel_input.inl`: sample R2 menu routing once on press, clear it when the world/manager changes, and retain the existing `[R2Route]` event trace.
- Protected-native x86 build/profile PASS. `test/build_native_menu_input_snapshot.ps1` 264/264 PASS; `test/build_eye_tracking.ps1` 29/29 PASS.
- `tools/deploy-r2-postload.ps1` installs only the new DLL after full nine-target backup, exact stock EXE/current DLL/profile checks and game-closed check. LOD 1.25 and previous NIS settings stay in the game INI.

## Next step

Install with game closed, verify nine backup hashes, then user tests R2 before and after death/load, plus pause-menu confirmation. Record log and headset verdict. No measured CPU/FPS claim.
