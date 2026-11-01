# Handoff: shotgun support grip, alignment, and camera shake

**Date**: 2026-11-01
**Task ID**: shotgun-twohand-shake-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed held-finisher candidate DLL `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF`, source `c9b86d0c`; its finisher headset result is still pending. Accepted rollback baseline remains `4a26aabd` per BASELINE.md.
**Integration commit**: pending; read-only diagnostic source `1e0925e9`, deployment script `1680fb00`

## Goal

The next gameplay candidate should let the left hand support an equipped shotgun, align its visible model and firing direction with the physical aim, and suppress camera shake without leaving hands or weapon shaking against a steady scene.

## Status

In Progress. A read-only pose observer is installed; no shotgun grip, alignment, or shake behavior has changed. Headset evidence is pending.

## Context Loaded

- [x] BASELINE.md and WORKFLOW.md
- [x] FAILED-APPROACHES.md (no shotgun or shake entry)
- [x] Current protected weapon/hands source; extracted `CameraShake.lua`, `DarknessVoice.lua`, `DrunkWalk.lua`, `Stoned.lua`; `trilex_vr_mod.md` camera/two-hand routes

## Findings

- `src/protected_hands.inl` publishes independent left grip and right gun aim poses. `src/native_hands.inl` explicitly leaves `support=false` for the protected packer. The legacy support calibration exists but is not a proven shotgun fore-end grip.
- `src/protected_weapon.inl` replaces the owned gun model and final shot matrix using the right controller aim pose. No shotgun-specific model-axis calibration exists. The latest installed log has no shotgun pose measurement, so a fixed rotation is not justified.
- User clarification: shotgun rounds already follow the right controller; **only the visible model points upward**. Keep the protected final shot ray intact. Derive a model-only correction from a live shotgun pose and screenshot.
- User screenshot `C:/Users/Trilex/AppData/Local/Temp/codex-clipboard-fa56ee73-5f9a-4d07-abc1-3609adf19f62.jpg`: the held shotgun is nearly vertical at the bottom centre of the VR view, consistent with the model-only report. The screenshot gives no native matrix or proven fore-end coordinate. At receipt, the last `d3d9.log` was 13:35, before the observer was installed at 14:03; a fresh run is needed.
- The game's scripts feed `postProcess.viewShake` through several independent effects. `CameraShake.lua` writes `mShakeAmbient`; `DarknessVoice.lua`, `DrunkWalk.lua`, and `Stoned.lua` also write shake fields. Disabling only one script would miss others. A late view-only cancellation risks relative weapon/hand motion; identify and suppress the shared native shake producer before tracked hand and gun transforms consume the camera frame.
- The stock EXE contains the `mShakeAmbient` property name at VA `00F6A514`; static references at `010A391C` and `010B9BCC` appear in registration tables. The registered setter `00DC8F50` writes float field `+0x2C` of a script-resolved view-shake object at `00DC8FFC`, and the getter `00DC9030` reads it at `00DC90AA`. These are evidence for one scripting property, not the native camera-shake consumer or a safe patch site.
- A separate native field is named `mFPEntityIgnoresShakeTranslationAndRotation` (VA `00FB8900`); its description at `00F87CD8` says the first-person entity ignores shake translation and rotation from animation and other sources. This is promising for keeping the weapon/arms stable, but the owning object and live use are not verified; do not write the field by name alone.

## Changes

- `src/protected_weapon.inl`: diagnostic `[WeaponPose]` at most once per second for the owned primary model, recording native and controller-driven world axes, weapon class, and left grip state. No model/shot/hand output changed.
- This handoff.
- `tools/deploy-shotgun-pose-observer.ps1`: protected nine-target backup/install from the held-finisher candidate; no INI/cache edits.

## Offline checks

- `build.ps1 -DiagnosticCapture` protected x86 build PASS outside the sandbox (sandbox linker alone refused its MSPDB140.DLL version).
- `tools/assert-candidate-profile.ps1` PASS for current repo, game and local profiles; DLL SHA256 `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`.
- `git diff --check` PASS. No Steam launch or game file change.
- Protected installer preflight PASS. MAIN installed the observer package without launching the game. Independent post-install check: backup hashes 9/9, live protected targets 9/9, stock EXE unchanged, game closed.

## Installed MAIN observer receipt

- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-pose-observer-20261101T140317414`.
- DLL SHA256 `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`, source `1e0925e9` plus documentation/deployment commits. Installed prior DLL `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF` is saved.
- Rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-pose-observer-20261101T140317414/rollback/Restore.ps1`.
- Existing finisher fix remains in source; headset finisher test remains pending. This observer does not include the three requested gameplay changes.

## Next Steps

1. User enters a scene with shotgun for 15-20 seconds and holds the right controller straight ahead. Read `[WeaponPose]` from the new log. The screenshot has already been received. If convenient in the same run, also test the still-pending held-enemy finisher on physical A.
2. Identify the exact shotgun variant and fore-end support point. The user's feedback already isolates the visual model from the working shot ray. Use the log and visual result to derive model calibration, not a guessed offset.
3. Implement one coherent two-hand grip state for shotgun only: require fresh right/left tracking and intentional left grip near the proven fore-end; release on tracking loss, unequip, grab conflict, or cinematic. Keep model, shot ray and human IK in the same frame.
4. Trace the native `viewShake` consumer and camera/human/gun transform order before adding a shake-disable switch. Verify no independent arm or weapon oscillation during a known shake event.
5. Build, protect nine rollback targets, install only when the game is closed, and obtain user headset feedback before MAIN promotion.

## Blockers

No captured shotgun transform or shared native shake-consumer address yet. Thus the three requested behavior changes are not ready for installation.
