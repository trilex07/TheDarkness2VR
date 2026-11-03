# Handoff: first two performance stages

**Date**: 2026-11-03
**Task ID**: performance-candidate-20261103
**Role**: MAIN
**Branch**: codex/performance-candidate-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline source**: ba558430 (plan), runtime B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2
**Runtime receipt**: cinematic-theater-20261102T193328001
**Integration**: pending headset acceptance; do not merge by inference.

## Goal and status

User authorized first two stages R1 and T1, persist through completion and
prepare a test candidate. Implementation/offline validation complete. R1 ready
for testing; T1 correctness PASS but performance-benefit gate NOT PASSED, OFF
in the recommended Cache profile. GPU work and resolution changes excluded.

## Context and evidence

Read BASELINE, workflow, relevant failures and retained performance plan/audit.
Detailed implementation, limitations, tests and next gate:
`docs/research/performance-candidate-20261103.md`.
Source changes in shader pool, replay worker/lifecycle, configuration and
package tooling. No EXE patch or native resource/AI/animation threading.

## Validation

Protected-native x86 build PASS. Actual-source worker65589, cache12503,
startup42+77, protected head117021,47/130-bone hands, both-flavor replay history/
constant order and pixel forwarding PASS. Private logs in build/. Offline
installer fixture PASS for four profiles, exact restore and rejection guards.
Standard build hits existing missing ProtectedShotgunSupportOwner definition;
final package uses the successful protected-native flavor.

## Next steps

1. See appended frozen package identity after preparation.
2. MAIN may install the frozen package when user opens the test window and
   game is closed. Default Mode Cache, numeric worker OFF. Never launch without
   authorization. User requested preparation in this turn, not a new recording.
3. Natural combat acceptance pending; no current FPS win claimed. Preserve
   full resolution and accepted rendering/input paths. Restore exact DLL/INI
   with package Install.ps1 -Restore if behavior regresses.
4. A useful broader worker requires larger proven immutable native work;
   do not simply enable more threads or turn the tiny-matrix prototype ON.

## Installation boundary

Installed game remains unchanged during preparation; no new capture or launch.
Repository defaults ShaderDemandCache=0 and ReplayPrepareWorker=0. Frozen
test profile Cache sets only1/0. Other profiles are explicit experiment controls.
