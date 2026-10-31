# HANDOFF: unarmed-trigger-dpad (side/unarmed-trigger-dpad)

- **Role**: MAIN (user-authorized build + Steam deployment 2026-10-31)
- **Base**: installed shouldrender-fix snapshot (half-rate 76b51a5a + HiddenSubmit/window/PresentModeChanged), DLL 5743F3F4
- **Branch**: side/unarmed-trigger-dpad-on-installed
- **Status**: READY FOR MAIN BUILD + IN-GAME TEST

## Goal
VR controllers have no D-pad. Stock pad: D-pad down = take weapon into hand, D-pad right = dual wield.
- Left trigger with EMPTY off (left) hand -> D-pad right (dual wield)
- Right trigger with EMPTY main (right) hand -> D-pad down (take weapon)

## Change
- `src/unarmed_trigger_dpad.h` - pure state machine (press >=128, release <=40, 120 ms D-pad pulse).
  Empty hand: trigger zeroed to the game. Translated press stays hidden until release, so it never becomes a shot once the weapon appears. Unknown state / no gameplay context / START held -> pass-through.
- `src/native_aim.inl` - `NativeGameplaySample::mainHandEquipped = **(DWORD**)(inventory+0xB0) != 0`.
  Evidence: existing `offHandEquipped` reads +0xBC with the same pattern; existing firearm loop reads `inventory+0xB0+12*hand` with hand 0 = primary/right, 1 = secondary/left. +0xB0 is slot 0 of that stride. **No new hook, no EXE patch.**
- `src/d3d9.cpp` - `ApplyUnarmedTriggerDpad(gp)` in `SyncOpenXRGamepad` right after the recenter chord, before protected grips/throw/gestures/adaptive triggers. INI `[Input] UnarmedTriggerDpad` (default 1).
- `Darkness2VR.ini` - key added with comment.
- `test/unarmed_trigger_dpad_test.cpp` + `test/build_unarmed_trigger_dpad.ps1`. Passed with g++ -Wall -Wextra -Werror (Linux VM); MSVC run pending on MAIN.

## Dependencies / risks
- Needs a fresh native gameplay sample (`CopyNativeGameplay`, <150 ms). Captured by the protected view-owner path (diagnostic `ImagePreservingInput` + protected gesture actions) or the native aim hook (`NativeLeftAim`/`NativeWeaponPose`). If neither runs, the log shows `unknown` and nothing changes.
- **Unverified at runtime**: that slot +0xB0/+0xBC is null exactly when the hand is empty (e.g. holstered weapons, scripted unarmed scenes, demon-arm-only moments). Check log.

## MAIN test steps
1. `test/build_unarmed_trigger_dpad.ps1`, then `build.ps1 -Install`.
2. In level, log `[UnarmedTriggerDpad] native hands main/right(+B0)=... off/left(+BC)=...` must change correctly when drawing/holstering/dual wielding.
3. One gun in right hand: pull left trigger -> second gun (dual wield). Empty hands: right trigger -> weapon drawn, no shot until released and pulled again.
4. If slot state is wrong -> `UnarmedTriggerDpad=0`, record in FAILED-APPROACHES.
