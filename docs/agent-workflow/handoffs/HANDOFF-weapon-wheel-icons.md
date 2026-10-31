# Handoff: weapon wheel icon silhouettes (model-specific revision)

**Date**: 2026-10-31
**Task ID**: weapon-wheel-icons
**Role**: SIDE
**Agent**: graphics SIDE (Claude, fork of MAIN session)
**Branch**: codex/hud-head-tentacles
**Worktree**: C:/Users/Trilex/.codex/worktrees/hud-head-tentacles/Darkness2VR
**Baseline**: accepted 4a26aabd; current diagnostic source e901b6ab, installed
DLL C306DA8A recorded in BASELINE.md; no runtime behavior inferred from assets.
**Integration commit**: pending MAIN (SIDE does not commit in the shared checkout).

## Goal
Icons for the user-requested dual weapon wheels. User rejected the first generic
chunky set and chose detailed pale silhouettes of the concrete Darkness II
weapon models. No game input/renderer source or installed game changes.

## Status
Ready for Review.

## Changes (assets/weapon-wheel/icons/ only, plus this handoff)
- `model_silhouettes.py`: 12 player weapons (roster from imfdb.org The_Darkness_II):
  M1911, Cobra, DesertEagle, MicroUzi, Ump45, Mp7, Defender, SawnOff, M1887,
  Kalashnikov, Carbine, Striker, plus EmptyHand. Outer contours + cut-outs,
  hand-authored, never traced; convex decomposition with greedy merging.
- `generate_icons.py`: enum appended (generic 0..5 unchanged, models 6..17,
  `Count` 18). Generic families now draw a representative model silhouette
  (Pistol=M1911, Revolver=Cobra, Smg=MicroUzi, Shotgun=Defender,
  AssaultRifle=Carbine). New helpers `Family()`, `TwoHanded()`, `Name()`.
  Budgets raised from 12 polygons/7 points to 64 polygons/8 points per icon,
  capped at 80 triangles/icon and min fan-triangle area 0.02.
- Regenerated `weapon_icons.h`, 18 SVGs, `preview.svg/png`, `revision-preview.png`.

## Evidence
- Generator PASS: bounds, convexity, triangle area, budgets for all 18 entries
  (max 64 triangles, Ump45).
- Actual `test/weapon_wheel_graphics_test.cpp` against the regenerated header
  (g++ 12 -std=c++17 -Wall -Wextra -Werror, fopen_s shim): PASS 60,750,001
  checks; max vertices/wheel 3621 <= 4096; dual 5034. MSVC x86 build via
  `test/build_weapon_wheel_graphics.ps1` not run by this SIDE (Linux VM) - MAIN to run.
- Header compiles -pedantic C++17 and C++20; static_asserts on enum order
  (AssaultRifle==5, Count==18), Family and TwoHanded.
- `revision-preview.png` inspected at 256 px and 48 px: all models distinct
  (pistol trio separated by slide/grip/hammer shape; revolver cylinder and
  hammer; SMG mag/stock layouts; pump vs pistol-grip vs lever shotguns; curved
  AK mag, M4 carry handle/front sight, bullpup mag-behind-grip). Wheel mesh from
  the test rendered and checked: icons sit inside sectors, readable.

## Notes for MAIN
- Test code already referenced `Icon::Count`, `M1911`, `MicroUzi`, `Defender`;
  these now exist, so the regression builds again.
- SVG previews show hairline AA seams between pieces; opaque ink without edge
  AA (or caching the wheel to a texture) avoids them in the HMD.
- `TwoHanded()` is a hint for presentation only. Mapping native inventory guns
  to model icons still needs owned gun identity evidence (see
  docs/research/weapon-wheel-native-inventory.md); do not use ObserveFirearm.

## Blockers
None for assets. HMD size/contrast check pending first in-game wheel build.
