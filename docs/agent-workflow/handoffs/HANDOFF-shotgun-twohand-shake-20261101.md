# Handoff: shotgun support grip, alignment, and camera shake

**Date**: 2026-11-01
**Task ID**: shotgun-twohand-shake-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed held-finisher candidate DLL `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF`, source `c9b86d0c`; its finisher headset result is still pending. Accepted rollback baseline remains `4a26aabd` per BASELINE.md.
**Integration commit**: pending; read-only diagnostic `1e0925e9`, rejected model-only trial `eb00b9d6`, deployment script `809b98c9`, source rollback `f9a902d6`

## Goal

The next gameplay candidate should let the left hand support an equipped shotgun, align its visible model and firing direction with the physical aim, and suppress camera shake without leaving hands or weapon shaking against a steady scene.

## Status

In Progress. The model-only headset trial failed and was rolled back 9/9. Installed game has the preceding read-only observer DLL `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`. A new shotgun aim/support candidate is in source behind `Diagnostics/ShotgunCandidate=0` and has no headset result. Camera shake suppression remains open.

## Context Loaded

- [x] BASELINE.md and WORKFLOW.md
- [x] FAILED-APPROACHES.md (no shotgun or shake entry)
- [x] Current protected weapon/hands source; extracted `CameraShake.lua`, `DarknessVoice.lua`, `DrunkWalk.lua`, `Stoned.lua`; `trilex_vr_mod.md` camera/two-hand routes

## Findings

- `src/protected_hands.inl` publishes independent left grip and right gun aim poses. `src/native_hands.inl` explicitly leaves `support=false` for the protected packer. The legacy support calibration exists but is not a proven shotgun fore-end grip.
- `src/protected_weapon.inl` replaces the owned gun model and final shot matrix using the right controller aim pose. No shotgun-specific model-axis calibration exists. The latest installed log has no shotgun pose measurement, so a fixed rotation is not justified.
- Initial user report: before the model-only trial, shotgun rounds appeared to follow the right controller while its visible model pointed upward. The follow-up trial showed the model and firing cannot safely be treated as independent without tracing native coupling.
- User screenshot `C:/Users/Trilex/AppData/Local/Temp/codex-clipboard-fa56ee73-5f9a-4d07-abc1-3609adf19f62.jpg`: the held shotgun is nearly vertical at the bottom centre of the VR view, consistent with the model-only report. The screenshot gives no native matrix or proven fore-end coordinate. At receipt, the last `d3d9.log` was 13:35, before the observer was installed at 14:03; a fresh run is needed.
- Fresh 14:12 observer log: the earlier 28 `[WeaponPose]` lines were for the old right-hand `firearm=5` gun, not the shotgun. On equipping the two-handed slot-0 gun `3779E370`, its model becomes `3EDEF360` with known model table `00FD6BC0`, while its fire action has table `00FD9D68` and type `010F4F58`. `[ProtectedWeapon] REFUSED` confirms the current generic pose/shot owner check rejects that action. `[PSVR2] right firearm=none` shows the haptic classifier also does not recognize the shotgun variant. The user's report that shots aim correctly is observational; do not hook or redirect this distinct native fire action.
- Static stock EXE check: `00FD9D68` points to code entries starting `00520F80`. The live action type at `010F4F58` is in runtime data and was observed in the log. This justifies class gating for a model-only trial, not any fire-action patch.
- Further static table check: shotgun action `00FD9D68` slot `+0x64` points directly to the shared final fire consumer `00C47570`; pistol action `01010AE0` slot `+0x64` points to `00CFBBA0`, whose final call reaches the same consumer. `00C47570` takes the fire receiver and five stack arguments including a matrix pointer, but the shotgun caller that supplies this matrix has not been identified. The table relationship alone does not prove which transform drives pellets.
- Stock EXE static search found the only direct relative call to `00C47570` at pistol `00CFBD60`; shotgun reaches it through an indirect dispatch. The exact shotgun call site and matrix argument remain unresolved.
- Stock EXE shotgun table at `00FD9D64` has a zero header, 33 executable entries, slot `+0x64` -> `00C47570`, then a null terminator. SHA256 over header plus all 33 entries is `0e44972a97d41330d8e977ed7327789e6afc700e774cc4b4200d09482b2e7c6f`. The shared consumer has the same five-argument ABI used by the existing controller-shot bridge. This supports a private shotgun action table trial without writing EXE bytes.
- The game's scripts feed `postProcess.viewShake` through several independent effects. `CameraShake.lua` writes `mShakeAmbient`; `DarknessVoice.lua`, `DrunkWalk.lua`, and `Stoned.lua` also write shake fields. Disabling only one script would miss others. A late view-only cancellation risks relative weapon/hand motion; identify and suppress the shared native shake producer before tracked hand and gun transforms consume the camera frame.
- The stock EXE contains the `mShakeAmbient` property name at VA `00F6A514`; static references at `010A391C` and `010B9BCC` appear in registration tables. The registered setter `00DC8F50` writes float field `+0x2C` of a script-resolved view-shake object at `00DC8FFC`, and the getter `00DC9030` reads it at `00DC90AA`. These are evidence for one scripting property, not the native camera-shake consumer or a safe patch site.
- A separate native field is named `mFPEntityIgnoresShakeTranslationAndRotation` (VA `00FB8900`); its description at `00F87CD8` says the first-person entity ignores shake translation and rotation from animation and other sources. This is promising for keeping the weapon/arms stable, but the owning object and live use are not verified; do not write the field by name alone.

## Changes

- `src/protected_weapon.inl`: diagnostic `[WeaponPose]` at most once per second for the owned primary model, recording native and controller-driven world axes, weapon class, and left grip state. No model/shot/hand output changed.
- Rejected trial at `eb00b9d6`: separate exact shotgun model owner for `00FD9D68` published the model vtable and right-hand aim pose, leaving the fire function itself untouched. Source was restored to pretrial observer code after user failure. Retain the trial commit as evidence only.
- New gated source trial: clone the exact 33-entry shotgun action table and replace only its final-fire slot with the existing five-argument bridge. The bridge supplies the right controller matrix to `00C47570`; the model resolver refuses shotgun replacement until that same live action has successfully used the bridge at least once. The protected hand packer captures the native animated left wrist relative to the right wrist for the selected shotgun, and a nearby left squeeze attaches the left hand to that pose. Input consumes that squeeze until release so it cannot become a demon-arm grab mid-hold. This is an offline candidate, not an accepted visual, hit, or two-hand result.
- `Diagnostics/ShotgunCandidate` defaults OFF. The opt-in source requires the exact shotgun action table/type, selected gun and model links, fresh native gameplay owner, live tracking and selected avatar packer. The 33-entry table digest pins the original method layout. Protected rollback covers the DLL, game/local INIs and caches.
- This handoff.
- `tools/deploy-shotgun-pose-observer.ps1`: protected nine-target backup/install from the held-finisher candidate; no INI/cache edits.

## Offline checks

- `build.ps1 -DiagnosticCapture` protected x86 build PASS outside the sandbox (sandbox linker alone refused its MSPDB140.DLL version).
- `tools/assert-candidate-profile.ps1` PASS for current repo, game and local profiles; DLL SHA256 `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`.
- `git diff --check` PASS. No Steam launch or game file change.
- Protected installer preflight PASS. MAIN installed the observer package without launching the game. Independent post-install check: backup hashes 9/9, live protected targets 9/9, stock EXE unchanged, game closed.
- Model-only follow-up: `build.ps1 -DiagnosticCapture` x86 PASS; profile gate PASS; candidate DLL SHA256 `5389DF98690B1A4D67E4585329E1DD525DE257C7F1E95E0B187A6DA5B7101BDF`.
- Model-only deployment: first installer attempt safely refused because `DarknessII.exe` was still running; no game files changed. After the user reported it closed, installation succeeded. Independent post-install audit: nine backups correct, 9/9 live targets matched expected hashes, stock EXE unchanged, game not launched by the agent.
- Headset trial: user reported the model now pointed forward, but shots could not hit and fingers did not reach the trigger; screenshot `C:/Users/Trilex/AppData/Local/Temp/codex-clipboard-9c2f1f89-5202-4c25-9790-4f5fdbb07067.jpg` shows the forward model and finger offset. Log contains `[ProtectedWeapon] shotgun` and `[WeaponPose] shotgun=1` with native/tracked axes often close; shotgun `firearm=0` remains unclassified by haptics. The native fire action was not patched, so the loss of hits suggests indirect coupling to model state (inference, not traced call).
- After user closed the game, the protected Restore.ps1 returned the prior nine targets; independent check confirmed 9/9 hashes, DLL `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`, stock EXE and no running game.
- Gated aim/support source trial: `build.ps1 -DiagnosticCapture` x86 PASS; profile gate `protected-native` PASS; DLL SHA256 `00F659DF51A7D3ED43084A837D190F2E288BB05861A464E31721FA32DDA99139`. Headset result NOT RUN. Native camera-shake suppression is not in this DLL.

## Installed MAIN observer receipt

- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-pose-observer-20261101T140317414`.
- DLL SHA256 `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`, source `1e0925e9` plus documentation/deployment commits. Installed prior DLL `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF` is saved.
- Rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-pose-observer-20261101T140317414/rollback/Restore.ps1`.
- Existing finisher fix remains in source; headset finisher test remains pending. This observer does not include the three requested gameplay changes.

## Rejected and rolled-back MAIN model-only trial receipt

- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-model-trial-20261101T141729420`.
- DLL SHA256 `5389DF98690B1A4D67E4585329E1DD525DE257C7F1E95E0B187A6DA5B7101BDF`; source `eb00b9d6`. Previous observer DLL `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C` saved.
- Rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/shotgun-model-trial-20261101T141729420/rollback/Restore.ps1`.
- Headset result FAIL: model visually forward; hits failed; right fingers missed the trigger. Rollback PASS 9/9. Left support and camera shake were not changed in this trial.

## Next Steps

1. Trace shotgun action `00FD9D68` to the actual pellet origin/direction and determine how it consumes the model matrix or socket. Observe the native shot and visible muzzle in the same frame before changing gameplay transforms again.
2. Identify the fore-end support point and right trigger hand socket in the shotgun's native skeleton/model. Any display adjustment must preserve the shot frame; do not reapply the rejected generic model-only override.
3. Implement one coherent two-hand grip state for shotgun only: require fresh right/left tracking and intentional left grip near the proven fore-end; release on tracking loss, unequip, grab conflict, or cinematic. Keep model, shot ray and human IK in the same frame.
4. Trace the native `viewShake` consumer and camera/human/gun transform order before adding a shake-disable switch. Verify no independent arm or weapon oscillation during a known shake event.
5. Build, protect nine rollback targets, install only when the game is closed, and obtain user headset feedback before MAIN promotion.

## Blockers

The shotgun's model-to-shot alignment and right trigger socket still need headset validation. The support grip currently places the left hand at the shotgun's stock animated fore-end but does not let left-hand motion steer the barrel. The shared native camera-shake producer and relative arm/gun order remain unproven. Do not install this source as a completed three-feature candidate or claim shake suppression.
