# Handoff: Circle guidance and native pause hand recovery

**Date**: 2026-11-05
**Task ID**: objective-pause-resume-20261105
**Role**: MAIN
**Branch**: codex/objective-pause-resume-20261105
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: accepted Steam DLL9476A8D5 / diagnostic117230AC; source76657702.
Parent candidate4dca29fe / documentation1b661948 is inherited, not accepted.
**Integration commit**: pending user acceptance

## Goal / status

Candidate prepared for Circle tap/hold separation and hands recovering after
double Triangle -> native Continue. No main merge or headset installation.
User's shrine element may be a 2D marker: its identity remains UNCONFIRMED.
Do not claim the inherited material-eye correction fixes that UI element.

## Context loaded

Bootstrap, BASELINE, WORKFLOW, relevant FAILED-APPROACHES, parent task handoff,
objective marker handoff, input systems and game automation skills.

## Changes / evidence

- src/vr_input.h: a fresh Circle press clears the preceding tap pulse, preventing
  tap -> quick hold from leaking B. After one second BACK remains held until
  release, instead of a single120ms pulse. Native menu/choice contexts preserved.
- src/d3d9.cpp, src/protected_left_throw.inl: protected gesture/hands gates observe
  existing MenuConfirmActive native pause state, rather than toggling on START.
  Native Continue has no START edge: the previous toggle stayed true and blocked
  ProtectedHandsAvailable/ProtectedWeaponAvailable until the next pause request.
  Reset recognizers on native pause transition; suppress attacks on START frame.
- Regression tests cover repeated native pause/Continue, short tap followed by
  hold, sustained guidance and no B on hold release. Host extraction has Log stub.
- README.txt documents tap/hold and native Continue behavior.

No new native offsets, executable patches or global render changes.

Follow-up: native_hud.h/.inl temporarily releases HideNativeHUD for eight
seconds after held Circle (and extends while held). Restores saved native
visibility only; never forces an originally hidden native HUD on. Native HUD
owner/override/lease tests472 PASS. ObjectiveRoute logs actual engine-facing
BACK and selected source, not just the requested XR state.

## Validation

- PASS: normal and DiagnosticCapture builds; diagnostic DLL SHA256
  065B5C9E838AC7E8C5D85C8ECC585E36B9C6D1FA0E212BB708D8AF22F1A9C030.
- PASS: cinematic/input regression and VR Settings suites4564/995/50/7215.
- PASS simulator startup, current Continue save, three double-Triangle/native
  Continue cycles. NativePauseInput1->0 each cycle; NativeHands right=1,left=1
  continues afterwards. Changed controller poses affect visible hands.
- PASS input delivery: held Circle reaches ObjectiveButton and requests BACK
  without B, logged at local d3d9.log line3738. However objective panel/purple
  route did NOT visibly appear in the captured scene. End-to-end guidance and
  the user's specific unlocked ability remain UNCONFIRMED, not runtime PASS.
- Private evidence: test-output/objective-pause-resume-20261105, paused.png,
  resumed-0/1/2.png, objective-held.png, d3d9.log, process.json, saves.json.
- Exact Steam DLL/diagnostic rollback PASS after lab; main INI/EXE/loader hashes
  unchanged; all four fresh user save/config originals restored and hash checked.
- Headset test NOT RUN. Do not infer hardware performance from simulator.

## Follow-up native consumer investigation (still OPEN)

Private test-output/objective-guidance-check-20261105/final-d3d9.log proves
BACK=1 source=XR buttons=0x0020, then BACK=0, and HUD hidden1->0. Native HUD
elements become visible. Neither repeated Select nor held Circle visibly
showed objective/purple route; do NOT call the complete Circle issue fixed.
An attempted250ms HUD pre-release did not resolve it and was removed.
Fresh originals restored again after the follow-up lab. Final frozen package
uses the rebuilt post-investigation DLL recorded in candidate.json, rather
than the earlier065B diagnostic build above. Headset remains NOT RUN.

Native stock EXE evidence (same pinned D46BB6CB identity): string
ForceShowCurrentObjective VA F4B720 referenced by6786D0 and A33630 handlers.
A33630 accepts action1A/down, first dispatches ten component handlers, then
tests avatar vtable+334 and inventory-related vtable+1AC result before calling
5CB990 and movie vtable+74 with ForceShowCurrentObjective. Exact bytes must be
pinned and owner/ABI established before any hook; no guessed native call added.
Stock controller manual confirms Back, PC manual Backspace; observed engine
delivery is not evidence that these later native conditions accepted the action.

## Candidate / next steps

Frozen package: build/objective-pause-resume-20261105/Install.ps1; Simulator OFF,
exact four-file rollback to accepted9476 baseline. Inherits shrine/menu/two-hand
candidate. VerifyOnly checks both payload and recovery hashes before installation.
Steam remains accepted9476, not the simulator or this prepared candidate.

Next: test Circle with user's unlocked ability and visible mission guidance;
if BACK delivery still fails, establish native action producer/rejection path.
Do not substitute guessed bindings, bypass choice/finisher ownership, or hide
unidentified NPC/shrine markers. Merge only after user acceptance.
