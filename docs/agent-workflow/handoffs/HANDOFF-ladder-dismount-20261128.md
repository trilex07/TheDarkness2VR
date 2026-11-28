# Handoff: Warehouse ladder dismount

**Date**: 2026-11-28
**Task ID**: ladder-dismount-20261128
**Role**: MAIN
**Branch**: codex/darkness2-ladder-dismount-20261128
**Worktree**: C:/Users/Trilex/Darkness2VR-ladder-dismount-20261128
**Candidate base**: 2eb3b747, tested comparison DLL E23CDE3B
**Integration commit**: pending

## Status

Passed the exact warehouse ladder mount, climb, top dismount and stationary
roof test in the simulator. The Steam installation and player save were
verified against the pre-test inventory after closing the private session.
Physical headset acceptance remains pending.

## Change

The selected camera-source bridge uses the original dispatch through native
traversal mode 6. The ladder top-end action at 821381 calls E09F40, which uses
the source owner's animated transform from its native +78 slot (BBC800).
The ordinary first-person stabilization had held that transform at the
ladder coordinates. The change leaves the game's own action and animation
responsible for where Jackie lands. It does not change the raw stick-axis
input already applied in mode 6.

## Evidence

- Steam witness recorded eight instances of the same player fall. The native
  camera reached (122.520, 0.250, 18.370), then fell straight down when mode 6
  ended. Private case `chapter-private-player-ladder` reproduced this with
  E23CDE3B and observed the action finish in state 4 at rung 10.
- Run `chapter-private-player-ladder-fix`, PID 40468, mounted the same action
  15 on the copied player save. The native camera source reached
  (121.086, 1.731, 18.271) before the action ended, a horizontal shift of
  about 1.44 m from the ladder. After held forward input was released Jackie
  remained in mode 0 on the roof at (118.380, 0.811, 18.497). The position
  was unchanged across samples taken 2.0 s apart after settling.
- Source built with `powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1 -DiagnosticCapture`.
  DLL SHA-256 `65BCC856F4545086D7FB1B45EA9C905832FB64F6474EC9219CA093DF8F73C262`.
  The exact Steam x86 ladder contract fixture passed using the shared simulator
  task's local Unicorn 2.1.4 dependency.
- Private capture root `C:/Users/Trilex/Darkness2VR-simulator-fixes-20261128/captures/simulator-20261128`.
  The successful `ladder-action-trace.jsonl` SHA-256 is
  `CE5CCB3B27D3DA2DC810F6AB327C25AC18D6B5F600C0757D252E42F324439FD6`.
  The successful `roof-stationary.jpg` SHA-256 is
  `D14AFF206628663EEA813FB92EA33268FF4B7132EE0651809BFFF27ACFC55CA5`.
  The case's `cleanup-verified.json` and `release-status.json` show the
  normal exit, restored baseline, and released lab.

## Next step

Carry the dispatch change into the tested integration candidate and verify
physical headset comfort during the native ladder animation.
