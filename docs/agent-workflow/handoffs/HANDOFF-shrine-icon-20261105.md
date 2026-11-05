# Handoff: purple shrine icon identification

**Date**: 2026-11-05
**Task ID**: shrine-icon-20261105
**Role**: MAIN
**Branch**: codex/shrine-icon-20261105
**Worktree**: .main-camera-shake
**Baseline**: accepted Steam source76657702, DLL9476A8D5; parent candidate ba8f1eb3, DLL7DD55BA4
**Integration commit**: pending; no production change from this investigation

## Goal
Remove the separate purple shrine overlay that follows head movement, keeping
the shrine flame, interaction and previously repaired native Continue hands.

## Status
In Progress. Exact user-visible element remains unconfirmed. No icon fix claimed.

## Context Loaded
- [x] BASELINE.md
- [x] FAILED-APPROACHES.md
- [x] Parent objective-pause-resume handoff and private runtime evidence

## Changes
No production code retained. The experimental HUD TalentBreadcrumb override was
removed after examining the actual SWF. Parent hands correction is unchanged.

## Evidence
- Private lab: test-output/shrine-icon-20261105 (not for Git publication).
- Actual HUD.swf is in B.AnimRetarget, not H.Misc; SHA256
  990478dde976e00bb7c6ef69235afe4cb6c90b6cec84686abf830130b083bfac.
- TalentBreadcrumb export/instance is character23, sprite at offset7464.
  Its PlaceObject2 creates character22 named Text; this is an animated text
  container. The Lua script only declares/resets mTalentBreadcrumb. This does
  not identify the user's purple shrine icon. Do not ship its hiding as a fix.
- Visible native movies in the observed shrine scene: HUD.swf,
  ObjectivePopUp.swf and SubTitlePopUp.swf. No separate shrine movie observed.
- ObjectivePopUp.lua only handles arrow types0/1/2/4; type1 is already hidden
  by the accepted patch. Do not infer a shrine arrow type3 from the gap.
- HUD root alpha is0.1 when hidden, but ContextActionPane uses compensating
  alpha100000. An interactive prompt inside that pane is not ruled out.
- Paired head-yaw captures showed the central shrine starburst following the
  surrounding world geometry. This does not prove the extra overlay is fixed.
- Selected PS E9CCEB07 / VS9635A564 is DangerousDecals base swirl: its observed
  pixel delta is the floor ellipse, not evidence of the requested icon.
- Relevant prefab references: TalentShrineHintProxy, TalentShrineHeartDeco,
  TalentMist, TalentGlyphSphere, TalentShrineBlackHoleEye, BaseLightShafts.
  Their mere presence is not proof that one produces the unwanted overlay.
- Single-frame capture metadata has an incomplete staged-result witness;
  individual draw data was inspected, not claimed as a full capture PASS.

## Recovery verification
Own simulator processes terminated with path/start-time identity checks.
Restored and SHA256 checked original DLL9476A8D5, diagnostic117230AC,
DLC H.Script.cache692FC3CD and toc0B9A879C. Protected EXE/main INI/loader
matched simulator-lab rollback checks. All four fresh user save/config backups
from this lab were restored and hash checked. No simulator candidate remains
installed. First cache-copy attempt encountered a transient file lock after
process termination; retry after confirming no game process succeeded.

## Next Steps
1. Obtain identification of the separate icon versus central radiant flame;
   clarification was sent while investigation continued.
2. Reproduce that exact element before adding another production patch.
3. Keep parent hands candidate available; do not mark Circle objective/trail
   visually fixed (the parent recorded engine input delivery only).

## Notes
Parent candidate build/objective-pause-resume-20261105 remains available.
No new install, main integration, headset acceptance or icon fix is claimed.
