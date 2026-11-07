# Handoff: simulator runtime continuation

**Date**: 2026-11-07
**Task ID**: simulator-continuation-20261107
**Role**: MAIN (user requested takeover of the running simulator audit)
**Branch**: side/mod-audit-20261107 (inherited task checkout)
**Worktree**: C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107
**Source**: c7cfe591; tested installed DLL 20FA6E8325273EEE83CC8172D8C69E629CF2D4F73755E4355D8D1704659E2041
**Production rollback**: A9D487B1C5D3B73930A9514C28C9B2CC643D35C1E6A997BBFA65C077E462CE72
**Integration**: pending; no main merge or new production deployment

## Result

Continued the predecessor's real Steam game test with the existing process-only
OpenXR simulator payload. Game was closed at takeover. Started owned PID3128
through Steam, entered Continue, tested crane gameplay, pause/resume, a head-pose
command and a right-trigger shot. Session ended; exact pre-lab files restored.
This is simulator evidence, not physical-headset acceptance or whole-mod coverage.

## Confirmed defect and correction

tools/measure-sim-arm.ps1 previously used --live-summary, which selects a window
ending at the newest timeline record rather than the current clock. Predecessor
G2-hidden reports exactly G1-gameplay's 1195 frames, 59.73 FPS and all timing
values despite only 2.7% renderer CPU and no new StereoPerf lines. It is invalid.
Do not infer that hiding the window improved performance. The cause of G1's
60-Hz cadence was not established and was not reproduced after restart.

Changed the sampler to --record (SkipToNow(1)) followed by --analyze. Rejects
fewer than two records, lost records, less than 80% of the requested duration,
recorder/analyzer failure and changed process identity. Names cannot escape the
lab directory; existing output names are rejected to preserve evidence.
CPU sampling includes recorder startup and its 150-ms final GPU drain, so its
wall window is slightly longer than the frame capture (~20.3 vs 20.1 seconds).

## Fresh runtime evidence

Private evidence: test-output/mod-audit-lab-20261107/ (not committed).

| Sample | Method | FPS | p50 / p99 ms | >11.111 ms | render CPU |
|---|---|---|---|---|---|
| R1-active | old live summary, independently advancing live log | 88.24 | 11.153 / 14.748 | 44.53% | 92.4% |
| R3-fresh-record | 1810 recorded frames, 20.100 s | 90.05 | 9.784 / 13.197 | 11.10% | 85.8% |
| R6-motion-combat | 1808 recorded frames, 20.085 s | 90.02 | 10.780 / 13.802 | 30.59% | 91.7% |

Views and scene state changed between samples; these are not matched performance
A/B arms. Remaining CPU pressure and over-budget frames persist. No stable
physical 90-Hz claim and no additional renderer optimization from this session.

- R1-left/right.png: separate eyes render world, hands, guns and tentacles.
- R2d-menu.png / R2d-pause-right.png: left-controller menu opens native pause;
  first-person geometry hidden in both eyes. Earlier keyboard attempts did not
  establish pause; R2/R2b filenames alone are not evidence of success.
- R3-resume.png: primary button resumes; hands/guns/tentacles return.
- R4-head-before / R4-head-after-left/right.png: view responds to simulator pose
  x=0.15 m, yaw=0.35 rad, pitch=-0.1 rad. Not a quantitative 6DoF accuracy test.
- R5-fire-held.png and takeover-game.log: right trigger delivered, native
  ProtectedWeaponShot stage=32 applied=1; AmmoHud right magazine falls to3/30.
- R7-rejection.txt: after closing simulator preview, native game PID3128 remains
  alive but XR stops. New sampler records only one boundary frame and rejects
  with "No complete fresh frame sample (empty or lost records)". No FPS summary
  produced. This reproduces the stale-ring failure boundary with the fix applied.
- git diff --check and PowerShell parser checks pass (see final verification).

## Recovery

Owned game PID3128, executable path and UTC start ticks639296507152821196 checked
before termination. Simulator-lab Restore PASS after process exit; initial
restore correctly refused while process exit was still pending.
DLL restored to A9D487B1; diagnostic INI117230AC, simulator OFF. Protected
EXE D46BB6CB, main INI22862511, loader D4DD992E match original lab receipt.
Four user files EE.cfg / Editor.cfg / CONTINUE.SAV / settings restored and
SHA256 verified against user-backup/receipt.json. Their post-test versions
preserved in takeover-before-recovery/. Final receipt: takeover-restored.json.

## Remaining scope

Further CPU profiling requires a fresh, mode-separated gameplay capture;
remaining over-budget frames are not solved. This run does not accept metro,
all interactions, full combat progression, graphics parity across all scenes,
or physical headset behavior. Continue from HANDOFF-mod-audit-20261107.md for
the earlier offline test changes. Production retains the previously installed
menu-cache candidate; audited test DLL was rolled back after this lab.
