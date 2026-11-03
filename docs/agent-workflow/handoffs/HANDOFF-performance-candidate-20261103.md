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

## Frozen candidate (prepared, NOT installed)

- Implementation commit c1cff287; installer permission fix ee4a2d0e.
- Frozen source ee4a2d0e5c3eae3b4f34bb38b30dd2dc74005087.
- Package: `C:/Users/Trilex/Darkness2VR/.main-camera-shake/build/performance-candidate-20261103-v2`.
- DLL SHA256 C393E422BC750D80EC753B978430A7074160437DA0080748BF279C5A0EE92F10.
- MAP SHA256 F19CD59EFC0F5D8DCAF415A587BC23BEED2FAFCF3365AE4AFCB2E0928ED4549E.
- PDB SHA256 2F03D9CB56E3154614A6D3342CAB76639E74799774F99C9DEBC6C9CAAA34E817.
- Cache INI SHA256 96BEA48305B4135A5B23169FFF11E12D8642CFE83B423445384511A480C45695.
- Frozen package VerifyOnly PASS; all four profiles independently checked to
  differ from exact baseline only in the two new switches. All installed DLL,
  INI and protected hashes still match baseline. No Steam/game launch or install.
- Use v2: original private package used CIM process enumeration, which this
  sandbox denied. v2 uses ordinary Get-Process enumeration and passes real
  read-only verification plus the offline busy-game refusal fixture.
- Install when authorized/closed: `powershell -ExecutionPolicy Bypass -File "C:\Users\Trilex\Darkness2VR\.main-camera-shake\build\performance-candidate-20261103-v2\Install.ps1"`.
- Same script `-Mode Off` selects original feature switches with this DLL;
  `-Restore` restores the exact prior baseline DLL/INI. No automatic rollback.
- Build/test logs and binaries remain private/untracked; source/tests/docs are
  committed on the task branch. Do not stage build/ or test-output/ wholesale.
