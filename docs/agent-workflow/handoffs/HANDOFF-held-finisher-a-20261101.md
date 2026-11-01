# Handoff: physical A during a held enemy finisher

**Date**: 2026-11-01
**Task ID**: held-finisher-a-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: user-approved small-tentacle candidate DLL `EFDCCFB0B40F37F716014DF836F5BE127121177B5C88EABC3E3714B828375B9C`; stock EXE `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
**Integration commit**: `c9b86d0c`

## Goal

Route physical right-hand A to the game's stock contextual A action while an alternate native target is held, without disturbing ordinary A-as-reload, menus or the double-X weapon switch.

## Status

Protected candidate installed with full nine-target rollback. Headset finisher result pending.

## Evidence

- User reports finisher on A fails while holding an enemy. Current protected build maps physical A to configurable X reload except when a fresh native sample has `demonHold==2`. `CaptureNativeGameplay` also publishes `alternateHeldTarget` for native `+584` character, `+590` retained target, or marked `+580` entity; those can have `demonHold==1` while an alternate native hold exists.
- Exact installed-candidate log shows `[NativeGrab] alternate character/weapon hold=1` before `[PadMap] AButton=X (buttons=0x4000)` and several `[DoubleTapDpad] X double tap` weapon-switch events. This proves A was routed through X on at least one press during the alternate hold. A later `[PSVR2] left tentacle=impale` proves some A reached the proxy's haptic decision, but not that the engine received or acted on it. Full finisher failure cause remains open.
- Candidate changes only `PhysicalAButton`: fresh `demonHold==2` **or** `alternateHeldTarget` now yields stock `XINPUT_GAMEPAD_A`. Normal non-held gameplay retains configured `AButton=X`; menu confirm retains A. No target, animation, native code, EXE or inventory data changes.
- Added bounded `[PhysicalA]` log on physical press/route change and `[FinisherRoute]` log when the XInput hook actually returns an A edge, recording XR versus physical controller precedence. These distinguish routing from native game action acceptance in the next test.
- Protected-native x86 build PASS, profile gate PASS, DLL SHA256 `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF`. Existing dual weapon wheel fixture 137 checks PASS. No game launched for this candidate.

## Next Steps

1. User grabs an enemy, presses physical A once and reports whether the finisher starts. Check `[PhysicalA] route=0x1000` and `[FinisherRoute] forwardedA=1 source=XR` on the same run. Also verify A still reloads outside a hold and double-A does not switch weapon while an enemy is held.
2. If the engine received A but no finisher occurred, investigate the native contextual action/hold state rather than guessing another face-button mapping.

## Installed MAIN receipt

- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/held-finisher-a-20261101T134251667`.
- DLL SHA256 `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF`, source commit `c9b86d0cbb97b9c4e536e3dc2ccf6a54a4a3bb06`.
- Independent post-install audit: nine backup hashes correct, 9/9 installed protected targets correct with only DLL changed, game closed and not launched.

## Blockers

The exact native game response to a forwarded A edge has no runtime witness yet. The proxy's impale haptic is not proof of a successful finisher.
