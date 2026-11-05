# Handoff: purple shrine icon identification

**Date**: 2026-11-05
**Task ID**: shrine-icon-20261105
**Role**: MAIN
**Branch**: codex/shrine-icon-20261105
**Worktree**: .main-camera-shake
**Baseline**: accepted Steam source76657702, DLL9476A8D5; parent candidate ba8f1eb3, DLL7DD55BA4
**Integration commit**: a72c5714; private six-file candidate installed at user request

## Goal
Remove the separate purple shrine overlay that follows head movement, keeping
the shrine flame, interaction and previously repaired native Continue hands.

## Status
User confirmed two purple angular symbols in private lab turn4.png.
Visual removal PASS in simulator. Headset NOT RUN; shrine interaction/menu
entry NOT VERIFIED in this session. Production Steam candidate installed after
explicit user request on 2026-11-05; headset acceptance remains pending.

## Context Loaded
- [x] BASELINE.md
- [x] FAILED-APPROACHES.md
- [x] Parent objective-pause-resume handoff and private runtime evidence

## Changes
tools/build-hide-shrine-symbols.py zeros exactly EyeColor and HiLiteColor of
TalentShrineBlackHoleMat in BASE H.Misc. Pinned packed/raw resource hashes,
fixed serialized length, original shader/macros/mesh/other TOC entries retained.
tools/deploy-asset-repair.py supports the exact H.Misc pair for this package kind.
tools/install-menu-polish.ps1 supports six targets only for the explicit
menu-hands-shrine-symbols kind; legacy four-file packages remain unchanged.
Temporary expanded shader-constant instrumentation was removed.
Parent hands correction is unchanged.

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
- Confirmed lab: test-output/shrine-icon-confirmed-20261105 (private).
- Native BlackHoleEye PS C51C7310 consumes c14 EyeColor/c15 HiLiteColor;
  original colors match the serialized material. Isolated zero-color trial
  removed the confirmed symbols while smoke/rays/world light stayed visible.
- Comparison: base-shrine.png (symbols present, wrong GlyphMat trial) versus
  black-shrine.png (symbols absent); black-back-view.png and black-use-final.png
  show preserved other shrine effects at additional distances/head poses.
- Raw material SHA256 3cba240e1c4efa883cc08b963944ab962dd6ad0bad90130f9f27779dda32512d.
  Packed original SHA256 6b6da76bc0ac52d217114ae50b08b855ae8eb9e94689127126c76874bf0ae1ec.
- Wrong trials: GlyphMat zero colors in DLC/base did not remove target;
  RaisedMat shader replacement produced black spikes; degenerate ARaised/Sphere
  mesh trials left target visible. All rolled back, none included in candidate.
- Private build/shrine-symbols-20261105 payload matches the visually tested
  blackhole trial hashes exactly. Combined build/menu-hands-shrine-20261105
  includes parent DLL7DD55BA4 and this asset pair. Six payloads, exact rollback,
  installed baseline and protected EXE/INI/loader verification PASS.
- Parent hands: three Continue cycles PASS previously. Circle objective/trail
  remains OPEN; do not claim a visual guidance fix from native BACK delivery.

## Recovery verification
Own simulator processes terminated with path/UTC start-time identity checks.
Restored and SHA256 checked original DLL9476A8D5, diagnostic117230AC,
DLC H.Script.cache692FC3CD and toc0B9A879C. Protected EXE/main INI/loader
matched simulator-lab rollback checks. All four fresh user save/config backups
from this lab were restored and hash checked. No simulator candidate remains
installed. Base H.Misc, base B.Misc and DLC H.Misc experimental trials restored
to their exact backups. Latest recovery: confirmed lab recover.ps1 PASS.

## Next Steps
1. Candidate build/menu-hands-shrine-20261105/Install.ps1 is ready for MAIN
   deployment with game closed; -Restore recovers the full six-file baseline.
2. Verify shrine menu entry and physical headset appearance at acceptance.
3. Keep guidance issue OPEN, separately from icon and hands fixes.

## Notes
Parent candidate build/objective-pause-resume-20261105 remains available.
## Final Steam deployment (2026-11-05)
User requested "wrzuc na steam". Game was closed. Frozen six-file installer
completed; post-install VerifyOnly PASS for all payloads, exact rollback and
protected EXE/main INI/loader. DLL7DD55BA4, production diagnostic117230AC;
simulator OFF, resolution preserved. Local receipt:
build/menu-hands-shrine-20261105/installed.json. Main integration and headset
acceptance remain pending. Restore with the same Install.ps1 -Restore.
