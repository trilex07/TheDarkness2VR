# Handoff: optional head-relative locomotion and firearm laser

**Date**: 2026-11-08
**Task ID**: head-move-gun-laser-20261108
**Role**: MAIN (explicit user assignment retained)
**Branch**: codex/head-move-gun-laser-20261108
**Worktree**: C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107
**Baseline**: current installed throw candidate F98BFEFD, source 0449d252;
branch starts at 3802b065. Older BASELINE.md acceptance is distinct.
**Integration commit**: pending; no main merge or public release.

## Goal and behavior

Two independent checkboxes in VR settings > Sterowanie, both default OFF.
`[VRSettings] HeadRelativeMovement` and `WeaponLaser` persist in the same active
INI as the existing settings. They apply immediately and restore their previous
value if persistence fails. No startup config edits required.

Movement rotates only the XR left stick into the current head's horizontal
heading relative to the same level recenter reference used by native geometry.
It preserves the existing right stick, physical gamepads and menu ownership.
Vertical/invalid head poses leave the original input unchanged. Existing
tracking, gameplay, focus, cinematic, pause and wheel gates must admit movement.

Laser uses CopyProtectedWeaponHandPose, the same current owned firearm aim
and optional two-hand support pose as ResolveProtectedWeapon. Both admitted
hands can show a beam. Unknown/native-only weapons get no misleading fallback.
It is an 8 m, 2 mm red compositor direction preview, with no impact marker,
collision query or wall occlusion; native collision, spread and ballistics stay
unchanged. This is a material limitation, not a demonstrated exact impact spot.
Focus/tracking loss, pause, cinema, settings and weapon wheel suppress it.
Layer capacity/runtime limits respected; native stereo accounting remains
independent and settings/wheel acceptance keeps its previous semantics.

Independent swapchain reuses existing tested settings acquire/wait/release and
resource lifecycle. Only its first solid red texel is submitted in beam quads;
no native geometry, image caches, shaders or offsets modified.

## Context and evidence

Read baseline, workflow, failed-approaches index and prior throw handoff.
No new native addresses introduced. Protected weapon owner and fire consumer
contracts remain those already proved by the native weapon tests.

Fresh validation:
- settings/input/beam geometry: 5787 checks
- actual renderer including solid texel and release/resource cleanup: 1041
- actual settings host ownership routing: 50
- actual settings submission: 7215
- actual INI load/save, independent toggles, reload, failed-write rollback: 15
- actual laser append gates, both hands and runtime layer limits: 9248
- protected weapon actual source: 125256; exact native body 3200 invocations
- protected throw actual bodies: 384
- protected-native DLL build and assert-candidate-profile: PASS
- git diff --check: PASS

Build: build/gameplay-options-final-20261108/d3d9.dll
SHA256: 642704E5B885B53078C918B70D833C1B87655C0C3A9139F07F30700E60D84D4C.
Build uses D2VR_MATCHED_CAPTURE, /Z7 and /DEBUG:NONE for known MSPDB mismatch.

## Deployment

Source implementation committed as 45533917. Exact DLL-only package frozen,
installed as user-authorized MAIN and -VerifyOnly PASS.
tools/prepare-gameplay-options-candidate.ps1 pins installed F98BFEFD and backs
it up with eight protected files hashed. Frozen package:
build/gameplay-options-steam-20261108/Install.ps1. -Restore returns F98BFEFD.
Receipt: installed.json in the same package. Installed SHA256 is the
642704E5 hash above. Configs, EXE, loader and caches stayed byte-identical.
Simulator OFF; game closed throughout installation.

## Remaining acceptance

No Steam launch, simulator or physical headset test for these features yet.
Verify perceived locomotion heading, beam/visible gun alignment, supported
weapons, settings interaction, tracking loss and frame time in actual gameplay.
Do not claim headset acceptance or wall-hit precision from offline evidence.
