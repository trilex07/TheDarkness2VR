# Handoff: L1 inward heart pull

Date 2026-11-08. Task heart-pull-20261108. Role MAIN (user assignment persists).
Branch codex/heart-pull-20261108.
Worktree C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107.
Base 479b0fb9, installed language DLL 9F7222AE. Accepted BASELINE is distinct.
Read BASELINE/WORKFLOW/FAILED-APPROACHES; applied input-systems and verification.

## Request and behavior

User postponed ladder reproduction until locating the after-Swifty scene.
Add devouring hearts with L1 plus pulling the left controller toward the body,
in addition to the existing button. No synthetic gameplay test authorization.

- src/heart_pull.h recognizes one 18cm inward stroke in .08-1.0 seconds with
  at least 14cm head-relative contraction; ends within 65cm of head.
- Both actual controller motion and contraction required; head lean alone,
  outward stroke, tracking jump, stale clock or held grip at reacquisition fail.
- Release arms; squeeze65% starts, release35% rearms. At recognition stock X
  held up to1.2s while grip remains down; native contextual target selection.
- Integration after two-hand support selection and before protected throw.
  Requires fresh native empty-grab state, no alternate held character/weapon,
  tracked head/left grip, gameplay/focus, no menus/wheels/cinema/QTE/START/fire,
  no two-hand support. Unknown/busy native grab fails closed.
- Consumed stroke owns grip through release, resets pending throw, emits no LB
  throw from its release. Existing physical X is never removed or rebound.
- Gesture sends contextual X: without an eligible heart, the game may interpret
  it as reload. There is no verified heart-target predicate in current source.
  Do not advertise heart-only targeting or confirmed actual devouring.
- No native offsets, EXE bytes or game-owned tables patched by this feature.

## Validation and files

- test/build_heart_pull.ps1: PASS62 checks (inward, side-facing, one-shot,
  bounded hold, release ownership, tracking, menu, head lean, outward, NaN/gap).
- test/build_protected_left_throw.ps1: PASS384 actual-body checks and native
  character-throw byte/instruction contracts. No changes to that recognizer.
- Full protected-native x86 DLL build PASS. SHA256
  F11472BE86A02DBB166F4A3E0D9F05A958B3C5DA332FEA9F74454CA3737405DF.
- Build build/heart-pull-20261108/d3d9.dll. Isolated /Z7 /DEBUG:NONE used due
  known incompatible PDB failure, same matched-capture release flavor.
- Runtime headset/actual heart consumption NOT RUN. Ladder unresolved/deferred.

## Deployment

Source0706868c frozen in build/heart-pull-steam-20261108 and installed MAIN.
Post-install VerifyOnly PASS: DLL F11472BE matches payload, exact rollback
9F7222AE present, ten protected installed files unchanged. Receipt installed.json.
Rollback with that package's Install.ps1 -Restore while game is closed.
tools/prepare-heart-pull-candidate.ps1 pins prior DLL and protects those files.
No release, remote push or main merge authorized in this task.
