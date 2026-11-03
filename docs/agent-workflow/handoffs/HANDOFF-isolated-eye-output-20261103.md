# Handoff: persistent isolated right-eye output

**Date**: 2026-11-03
**Task ID**: isolated-eye-output-20261103
**Role**: MAIN
**Branch**: codex/isolated-eye-output-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: accepted Combined d58156b4, installed FE216AF4E8B310C9D7E68EB059428194BA49B1FDE1C448CD1547F7E0419FF219,
INI2537EC1EFE21509EC86AD18B1A878EBE607DB8B051A022C1D44C8DD3479CA14D;
receipt build/render-pipeline-candidate-20261103/installed.json.
**Integration commit**: none, experimental dedicated branch.

## Goal and status

User asks to continue larger optimizations after Combined trial. Implemented
an opt-in persistent private right RT0, removing four steady full-surface color
copies per admitted pair. No further tiny-worker expansion; no resolution,
stereo, hands/tentacles, camera or XR sequencing change. Offline correctness
PASS, package freezing follows commit. No measured game FPS improvement.

## Context loaded

Bootstrap (its default SIDE label does not override user's MAIN assignment),
BASELINE, WORKFLOW, relevant failures, Combined, engine-parallel-work,
present-overlap, shared-capture-color-reuse review/quarantine and current log.
Detailed ownership, source evidence, tests and limitations:
docs/research/isolated-eye-output-20261103.md.

## Changes and evidence

src/isolated_eye_output.h and isolated_eye_replay.inl own persistent output and
exact reversible record pointer patches; pair scheduler uses optional mapped
Snapshot Apply and original fallback. IsolatedEyeOutput defaults0. Reset releases
private output. New real-driver and expanded interop fixtures plus package factory.
Real D3D9Ex101169 checks, interop2860 eye transfers (1080 fullres), history,
snapshot2545, replay surfaces896 calls, constants/bindings/head/hands/tentacles
PASS. Private artifacts/logs under test-output/isolated-eye and build/.

## Next steps and boundaries

1. Freeze build/isolated-eye-output-20261103 with current accepted Combined as
   rollback. Record source/hash/verification below after preparation.
2. No new Steam installation or game launch authorized in this task. Current
   accepted Combined remains installed. Do not deploy automatically.
3. Default Combined adds ONLY IsolatedEyeOutput; Off switches only this new
   path off on the new DLL. -Restore returns exact accepted DLL/INI.
4. HMD/pacing/FPS and in-game eligibility NOT RUN. Require natural combat,
   partial/UI/cinema/history transitions and same-scene A/B. Native whole-list
   threading remains unimplemented. Keep ReuseSharedCaptureColor OFF.

## Frozen candidate receipt

Prepared, not installed:
C:/Users/Trilex/Darkness2VR/.main-camera-shake/build/isolated-eye-output-20261103

- Source81bddf2b4ec734a8fab5d885e01099b17752606e.
- Protected-native DLL E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA.
- candidate.json B8630F371CC3A1C3F5F35E1D34C6464D6D724DCC76873EA431307C87E0A46FB4.
- candidate.ini (Combined + IsolatedEyeOutput1)
  68EE5685C7AE9FEC8F1AE2677D551DD90C7BCC21DD66737560815EEF4AB26EA9.
- off.ini (accepted Combined switches + IsolatedEyeOutput0)
  855C1D89227BCD2C7FD03B87EB6CE61DA41C8FF04C9D0DB5B2719C4FCD000EC5.
- Exact baseline backups are accepted FE216AF4 DLL and2537EC1E INI.
- VerifyOnly PASS with game closed; payloads, baseline backups and every
  protected target verified. Independent A/B comparison differs by ONE switch.
- Standard final artifact backup: build/isolated-eye-standard-final-20261103,
  DLL9F3D5F959AC11A6DF249F423169CB48B2F667788C2E9146734B843A220FB698B.
- Both-flavor packet lifecycle942915/942916 rerun PASS. No new installer action,
  no game launch, no cache change; Steam DLL/INI rehashed unchanged.
