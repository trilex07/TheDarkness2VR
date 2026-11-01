# Handoff: shotgun support grip, alignment, and camera shake

**Date**: 2026-11-01
**Task ID**: shotgun-twohand-shake-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed held-finisher candidate DLL `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF`, source `c9b86d0c`; its finisher headset result is still pending. Accepted rollback baseline remains `4a26aabd` per BASELINE.md.
**Integration commit**: pending; read-only diagnostic `1e0925e9`, model-only trial `eb00b9d6`, deployment script `809b98c9`

## Goal

The next gameplay candidate should let the left hand support an equipped shotgun, align its visible model and firing direction with the physical aim, and suppress camera shake without leaving hands or weapon shaking against a steady scene.

## Status

In Progress. The read-only observer established that the shotgun's distinct native fire action blocks our generic weapon path. A model-only trial is installed, with headset result pending. Two-hand support and shake suppression remain open.

## Context Loaded

- [x] BASELINE.md and WORKFLOW.md
- [x] FAILED-APPROACHES.md (no shotgun or shake entry)
- [x] Current protected weapon/hands source; extracted `CameraShake.lua`, `DarknessVoice.lua`, `DrunkWalk.lua`, `Stoned.lua`; `trilex_vr_mod.md` camera/two-hand routes

## Findings

- `src/protected_hands.inl` publishes independent left grip and right gun aim poses. `src/native_hands.inl` explicitly leaves `support=false` for the protected packer. The legacy support calibration exists but is not a proven shotgun fore-end grip.
- `src/protected_weapon.inl` replaces the owned gun model and final shot matrix using the right controller aim pose. No shotgun-specific model-axis calibration exists. The latest installed log has no shotgun pose measurement, so a fixed rotation is not justified.
- User clarification: shotgun rounds already follow the right controller; **only the visible model points upward**. Keep the protected final shot ray intact. Derive a model-only correction from a live shotgun pose and screenshot.
- User screenshot `C:/Users/Trilex/AppData/Local/Temp/codex-clipboard-fa56ee73-5f9a-4d07-abc1-3609adf19f62.jpg`: the held shotgun is nearly vertical at the bottom centre of the VR view, consistent with the model-only report. The screenshot gives no native matrix or proven fore-end coordinate. At receipt, the last `d3d9.log` was 13:35, before the observer was installed at 14:03; a fresh run is needed.
- Fresh 14:12 observer log: the earlier 28 `[WeaponPose]` lines were for the old right-hand `firearm=5` gun, not the shotgun. On equipping the two-handed slot-0 gun `3779E370`, its model becomes `3EDEF360` with known model table `00FD6BC0`, while its fire action has table `00FD9D68` and type `010F4F58`. `[ProtectedWeapon] REFUSED` confirms the current generic pose/shot owner check rejects that action. `[PSVR2] right firearm=none` shows the haptic classifier also does not recognize the shotgun variant. The user's report that shots aim correctly is observational; do not hook or redirect this distinct native fire action.
- Static stock EXE check: `00FD9D68` points to code entries starting `00520F80`. The live action type at `010F4F58` is in runtime data and was observed in the log. This justifies class gating for a model-only trial, not any fire-action patch.
- The game's scripts feed `postProcess.viewShake` through several independent effects. `CameraShake.lua` writes `mShakeAmbient`; `DarknessVoice.lua`, `DrunkWalk.lua`, and `Stoned.lua` also write shake fields. Disabling only one script would miss others. A late view-only cancellation risks relative weapon/hand motion; identify and suppress the shared native shake producer before tracked hand and gun transforms consume the camera frame.
- The stock EXE contains the `mShakeAmbient` property name at VA `00F6A514`; static references at `010A391C` and `010B9BCC` appear in registration tables. The registered setter `00DC8F50` writes float field `+0x2C` of a script-resolved view-shake object at `00DC8FFC`, and the getter `00DC9030` reads it at `00DC90AA`. These are evidence for one scripting property, not the native camera-shake consumer or a safe patch site.
- A separate native field is named `mFPEntityIgnoresShakeTranslationAndRotation` (VA `00FB8900`); its description at `00F87CD8` says the first-person entity ignores shake translation and rotation from animation and other sources. This is promising for keeping the weapon/arms stable, but the owning object and live use are not verified; do not write the field by name alone.

## Changes

- `src/protected_weapon.inl`: diagnostic `[WeaponPose]` at most once per second for the owned primary model, recording native and controller-driven world axes, weapon class, and left grip state. No model/shot/hand output changed.
- Follow-up `src/protected_weapon.inl`: separate exact shotgun model owner for the live `00FD9D68` action, publishing only the already pinned model vtable. Its native fire action remains untouched; the right human hand is allowed to share the model's controller aim frame. New `[WeaponPose] shotgun=1` can compare native and tracked transforms. This is a trial pending headset alignment/shot checks, not a proven visual fix.
- This handoff.
- `tools/deploy-shotgun-pose-observer.ps1`: protected nine-target backup/install from the held-finisher candidate; no INI/cache edits.

## Offline checks

- `build.ps1 -DiagnosticCapture` protected x86 build PASS outside the sandbox (sandbox linker alone refused its MSPDB140.DLL version).
- `tools/assert-candidate-profile.ps1` PASS for current repo, game and local profiles; DLL SHA256 `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`.
- `git diff --check` PASS. No Steam launch or game file change.
- Protected installer preflight PASS. MAIN installed the observer package without launching the game. Independent post-install check: backup hashes 9/9, live protected targets 9/9, stock EXE unchanged, game closed.
- Model-only follow-up: `build.ps1 -DiagnosticCapture` x86 PASS; profile gate PASS; candidate DLL SHA256 `5389DF98690B1A4D67E4585329E1DD525DE257C7F1E95E0B187A6DA5B7101BDF`.
- Model-only deployment: first installer attempt safely refused because `DarknessII.exe` was still running; no game files changed. After the user reported it closed, installation succeeded. Independent post-install audit: nine backups correct, 9/9 live targets match expected hashes, stock EXE unchanged, game not launched by the agent.

## Installed MAIN observer receipt

- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-pose-observer-20261101T140317414`.
- DLL SHA256 `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`, source `1e0925e9` plus documentation/deployment commits. Installed prior DLL `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF` is saved.
- Rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-pose-observer-20261101T140317414/rollback/Restore.ps1`.
- Existing finisher fix remains in source; headset finisher test remains pending. This observer does not include the three requested gameplay changes.

## Installed MAIN model-only trial receipt

- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-model-trial-20261101T141729420`.
- DLL SHA256 `5389DF98690B1A4D67E4585329E1DD525DE257C7F1E95E0B187A6DA5B7101BDF`; source `eb00b9d6`. Previous observer DLL `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C` saved.
- Rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-model-trial-20261101T141729420/rollback/Restore.ps1`.
- User headset check pending: visible shotgun follows right controller, right hand remains attached, both eyes, and shots still follow original native aim. Left support and camera shake are not changed in this trial.

## Next Steps

1. User checks whether the shotgun model now points with the controller and whether shots still hit correctly. Read `[WeaponPose] shotgun=1` and check both eyes/hands.
2. Identify the exact shotgun variant and fore-end support point. If the generic controller frame still leaves the model vertical, derive a model-only orientation from the new log and screenshot rather than guessing a fixed offset. The native shotgun fire action stays unmodified.
3. Implement one coherent two-hand grip state for shotgun only: require fresh right/left tracking and intentional left grip near the proven fore-end; release on tracking loss, unequip, grab conflict, or cinematic. Keep model, shot ray and human IK in the same frame.
4. Trace the native `viewShake` consumer and camera/human/gun transform order before adding a shake-disable switch. Verify no independent arm or weapon oscillation during a known shake event.
5. Build, protect nine rollback targets, install only when the game is closed, and obtain user headset feedback before MAIN promotion.

## Blockers

The shotgun model trial is not headset validated; no fore-end support coordinate or shared native shake-consumer address yet. Thus the full three-feature candidate is not ready for acceptance.
