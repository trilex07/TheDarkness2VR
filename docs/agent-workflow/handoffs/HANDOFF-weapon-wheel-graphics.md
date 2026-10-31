# Handoff: dual hand weapon-wheel graphics

**Date**: 2026-10-31
**Task ID**: weapon-wheel-graphics
**Role**: SIDE
**Agent**: /root/weapon_wheel_graphics
**Branch**: codex/hud-head-tentacles
**Worktree**: C:/Users/Trilex/.codex/worktrees/hud-head-tentacles/Darkness2VR
**Baseline**: Accepted rollback4a26aabd; current diagnostic candidatee901b6ab,
DLL C306DA8AA8008514F1672BFC27628A799358A3C4AEE6B002241F2141A98A5C20.
**Integration commit**: pending MAIN; explicit assignment reserves commits to MAIN.

## Goal
Portable vector triangle geometry for one weapon wheel per hand. User clarified
hold R3 to show both wheels and physically move each hand toward its weapon.
No stick-selection, input rewriting or native equip logic belongs to this SIDE.

## Status
Ready for Review / integration. No runtime or headset acceptance claimed.

## Context Loaded
- AGENTS.md, SIDE bootstrap, BASELINE.md, WORKFLOW.md.
- FAILED-APPROACHES.md checked for wheel/HUD/overlay/UI failures; no relevant entry.
- Coordinated actual icon polygon API with weapon-wheel-icons SIDE.

## Changes
- `src/weapon_wheel_graphics.h`: self-contained C++17 vector triangle builder;
  includes the icon SIDE's `assets/weapon-wheel/icons/weapon_icons.h`.
- `assets/weapon-wheel/wheel-preview.svg`: actual emitted geometry, not a mockup.
- `test/weapon_wheel_graphics_test.cpp` and `test/build_weapon_wheel_graphics.ps1`:
  reproducible actual-header geometry regression (added on MAIN request).
- This per-task handoff. No existing source, config, baseline or deployment edits.

## Integration contract
- Namespace `d2vr_weapon_wheel_graphics`.
- `Vertex {float x,y,r,g,b,a;}`; callback receives three vertices per triangle.
  Colors are straight alpha; caller chooses blend/render state and restoration.
- `Wheel` has `count`1..8, `selected` and `equipped` indices (-1 none), `Slot[8]`
  with icon and availability. Invalid negative count yields no geometry, >8 clamps.
- `BuildWheel(wheel,leftHand,emit,Placement{x,y,radius})` emits local planar
  geometry. Default plane radius.9, centered0. Positive x right, positive y down.
  Root maps it into the anchored hand plane; no HMD/world matrices here.
- `SlotCenter(index,count,placement)` returns the icon center used by this exact
  geometry. Slot0 top, clockwise order. Radius of icon centers=.69*wheel radius.
  Root owns physical-hand projection/proximity, hysteresis and action commitment.
- `BuildDual(left,right,emit)` is a flat preview convenience only. Prefer separate
  `BuildWheel` calls for two independently anchored VR wheels.
- `MaxVerticesPerWheel=4096`, `MaxVerticesDual=8192`. Validated worst icon/state
  combination emits2520 vertices per wheel. No allocation, texture/font access,
  shader compilation or graphics API inside this header. Cache mesh on UI changes.
- Available icons use opaque white; unavailable icons opaque muted gray. Convex
  icon components intentionally overlap; opaque ink prevents overlap darkening.
- Dark translucent sectors, crimson hover fill, bone-white selected rim, crimson equipped
  dot, mirrored central hand glyph and geometric L/R. No text/font dependency.
- All triangle winding normalized to positive signed area in y-down coordinates.
  Root must account for its world-plane orientation/culling convention.

## Evidence
- MSVC x86 C++17 actual-header compile PASS.
- `./test/build_weapon_wheel_graphics.ps1 -Preview` compiles the actual header
  and runs count1..8, selection-1..count-1, both hands, all6 icon kinds and four
  availability patterns. Checks finite vertices, radius bounds, color/alpha
  bounds, nondegenerate consistent winding, capacity, icon centers, suppressed
  disabled highlights, equipped marker size, independent hand placement,
  invalid/nonfinite placement and count handling. PASS15,671,113 assertions;
  max2520 vertices/wheel. The dual preview emits4194 vertices. Optional Preview
  writes generated SVG under build; tracked artwork is never overwritten.
- Earlier local exploratory fixture PASS3,929,810 (superseded by durable test).
- Preview generated from the same emitted triangles, rasterized with Pillow and
  visually inspected at `build/weapon-wheel-graphics/wheel-preview.png`1280x720.
  Both labels, white silhouettes, selection and unavailable state readable.
- Exploratory fixture/binaries/PNG under build are generated local evidence, not source
  deliverables. MAIN should not accidentally stage them.
- No Steam launch, installed game file access, native patch or deployment.

## Next Steps
1. MAIN integrates held-R3 lifecycle and actual owned weapon selection.
2. MAIN creates/caches appropriate GPU buffers, world anchors and render-state
   restoration, respecting icon center order and straight-alpha blending.
3. MAIN validates per-hand reach selection, release commit/cancel, readable size,
   overlap, depth placement and existing controls on headset. Not tested here.

## Blockers
None for geometry integration. Native equip, GPU integration and headset tests
are MAIN-owned and outside this SIDE's scope.

## Visual revision requested by user
Original generic six-class silhouettes were rejected by user as ugly; icon SIDE
is replacing them with12 game-model-specific silhouettes plus EmptyHand. Do not
represent the initial generic preview as an approved design.

Graphics revised to dark ink/bone-white/crimson, thin1.4%R outer rim, thin1%R
inner rim, narrower slot gaps, smaller central hub and larger46%R icons. Runtime
API, slot centers and4096-vertex budget remain unchanged. Selected hover only
applies to available slots. Text labels are added to SVG preview only.

Illustrative preview inventory is M1911/MicroUzi/Defender/Empty in both wheels,
not the full gun catalogue. Left Defender shown unavailable; equipped left
MicroUzi/right M1911; hovered left M1911/right Defender. These demo states do
not establish native per-hand eligibility or equip acceptance.

New test iterates every distinct enum value0..Icon::Count-1, independent of old
compatibility aliases. Final rebuilt evidence pending icon SIDE's new header.
