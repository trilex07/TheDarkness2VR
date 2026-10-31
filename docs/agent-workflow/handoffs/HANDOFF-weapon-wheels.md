# Handoff: physical dual weapon wheels and crouch

**Date**: 2026-10-31
**Task ID**: weapon-wheels
**Role**: MAIN
**Agent**: /root, continued by Claude MAIN 2026-10-31 22:40
**Branch**: codex/hud-head-tentacles
**Worktree**: current
**Baseline**: accepted 4a26aabd; installed diagnostic source e901b6ab, DLL C306DA8A (see BASELINE.md).
**Integration commit**: pending

## Goal
Hold VR R3 to display two wheels, one per hand. Move each physical hand toward
its weapon icon, as in the user's Singularity implementation; sticks do not
choose weapons. Right stick down toggles native crouch, once per downward
deflection, with neutral rearm. Preserve existing double-X switching and
L3+R3 recenter. User requested graphics SIDE agents explicitly.

## Status
In Progress. Runtime integration written; host test PASS on g++ (139 checks).
MSVC DLL build, install and headset test still pending (user runs the build:
terminals are click-only for computer use). Not installed.

## Context Loaded
- [x] AGENTS.md, MAIN bootstrap, BASELINE.md, WORKFLOW.md, FAILED-APPROACHES.md.
- [x] User-invoked trilex_vr_mod.md and input-systems skill.
- [x] SingularityVR/spikes/view_matrix/weapon_wheel.h and weapon_wheel_layout.h.
- [x] src/native_aim.inl, src/vr_settings_renderer.inl, XR input/submission in d3d9.cpp.

## Changes
- Two user-requested SIDE graphics tasks: weapon-wheel-icons and weapon-wheel-graphics.
- Runtime-independent geometry src/weapon_wheel_graphics.h and generated icon header.
- Durable actual-header geometry regression and preview generator under test/.
- User rejected first generic chunky silhouettes: revise against in-game weapon
  screenshots, with model-specific, properly proportioned silhouettes.
- User explicitly chose detailed pale silhouettes of concrete models over
  colored miniatures. Keep that art direction in subsequent revisions.

## Implementation (Claude MAIN)
- `src/dual_weapon_wheel.h` (pure): HoldGate (R3 hold 220 ms opens, tap forwards a
  100 ms R3 pulse, L3 = recenter chord passes through), StickDownToggle (right
  stick <= -0.75 fires once, rearm above -0.30, Y hidden while engaged), hand
  sector from planar offset (dead zone 4.5 cm, slot 0 top, clockwise = graphics),
  Right/Left menus from the native loadout, Resolve -> stock DPAD action, Request
  with closed-loop verification.
- `src/weapon_wheel_raster.h`: CPU rasteriser, 2x2 supersampled, top-left fill
  rule (no seams), premultiplied BGRA. Rebuilt only on display change.
- `src/weapon_wheel_input.inl`: byte-verified loadout capture (called from
  CaptureNativeGameplay), identity probe, wheel anchors at each grip facing the
  head, hover, release -> request, crouch flick ([Input] CrouchButton, default
  R3, re-read live). Wheel owns triggers + right stick while open.
- `src/weapon_wheel_renderer.inl`: one 1024x512 swapchain, two quads, source
  alpha blend. Layers array 3 -> 5; skipped while the settings panel is shown.
- INI: [Input] WeaponWheel=1, CrouchStickDown=1, CrouchButton=R3 (all optional).
- Test: `test/build_dual_weapon_wheel.ps1` (MSVC /W4 /WX), same source PASS on g++.

## Evidence
- Existing native inventory ownership: camera->avatar+0x630; inventory vtable
  0xFFCA10 and inventory+0x2A8 strong reference back to selected local avatar.
- Native inventory RE findings: docs/research/weapon-wheel-native-inventory.md.
- Geometry SIDE initial regression: 15,671,113 checks PASS; <=2520 vertices per
  wheel. Re-run after icon revision; initial counts are not final acceptance.
- No Steam/game launch, input injection, or installed game writes for this task.

## Next Steps
0. Build protected-native (`build.ps1 -DiagnosticCapture`) + both host tests;
   deploy with full rollback only when the game is closed and the user agrees.
   Headset check list: R3 tap still does stock R3; hold shows wheels at hands;
   right wheel two-handed <-> one-handed; left wheel pistol ends two-handed;
   stick down toggles crouch (if not, set CrouchButton=B/L3 live); L3+R3 recenter.
   Read `[WeaponWheelProbe]` lines to map gun strings to model icons.
1. Visually review revised icons, run final actual-source geometry checks, commit.
2. Implement pure physical-hand selection/hold/release and downward-stick edge
   state with neutral rearm after focus/settings/recenter/tracking loss.
3. Prove owned inventory slot/type identification and stock equip request ABI
   before connecting to live gameplay. Do not use PSVR2 ObserveFirearm as the
   icon model classifier (shared charged-fire state is not gun identity).
4. Integrate two cached OpenXR quad surfaces; no closed-wheel work/allocation,
   native rendering edits, or new shader work per frame. Preserve settings
   input ownership, pause/cinematic cancellation, recenter priority and double-X.
5. Offline tests/build, protected profile package with full nine-target rollback,
   closed-game install if authorized; user launches/tests manually.

## Blockers
Gun type identity and legal per-hand native selection are not proven yet.
Graphics delivery is not proof of functioning inventory selection.

## Notes
Earlier lighting capture serial104 produced no capture files; user pivoted to
this task. World lighting/FPS acceptance remains unresolved; do not claim a fix.
