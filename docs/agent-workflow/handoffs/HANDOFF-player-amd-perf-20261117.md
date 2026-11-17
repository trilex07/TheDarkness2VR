# Handoff: recovered player AMD / VDXR performance

Date: 2026-11-17
Task ID: player-amd-perf-20261117
Role: MAIN (offline work only this turn)
Agent: /root
Branch: codex/darkness-player-perf-20261117
Worktree: C:/Users/Trilex/Darkness2VR-pico-stick-20261117
Source parent: 6945161f (optional Pico range calibration, default OFF),
9d4a961b controls lineage. This is not proof of the player's exact DLL hash.
Recorded baseline: docs/agent-workflow/BASELINE.md; no local Steam verification
or deployment was performed. Do not confuse recorded history with installed state.

## Goal and status

Locate the sustained gameplay cost on the player's RX 7600 / Xeon / Pico 4 /
VDXR setup. Read complete Downloads logs, not the earlier truncated pastes.
Offline diagnostic candidate ready for review; FPS fix NOT established.

## Findings

- VDXR instance/session and world layers succeed in the supplied run.
- Gameplay world cadence falls to roughly 41-56/s in sampled windows vs 72 Hz.
- right+restore CPU wall often 11-15 ms, worker mainJoin about 5-7 ms per job,
  worker body about 0.1 ms. These are overlapping scopes, NOT additive.
- NIS pair GPU dispatch about 0.3 ms; dropping sharpening is not a justified
  first action. GPUFrame is an elapsed timestamp span including submission
  gaps, not GPU Busy. SS=125 is not a proven regression cause without A/B.
- FinishStereoTransfer's existing mainJoin conflates DrainOwnerProducer,
  ArmStereoTransfer and Pending::Finish. Source order identifies where to
  subdivide; it does not prove which dominates on the remote AMD machine.

## Change

[Diagnostics] StereoTransferTiming=0 (default). Setting 1 splits existing
mainJoin into producerDrain, publish and workerFinish mean/peak per job.
Two extra QPC reads only on staged jobs while enabled. No GPU calls, waits,
resource ownership, replay, buffering, pixel dimensions, runtime selection or
query flags change. Successful jobs partition the same enclosing mainJoin;
workerMean overlaps and must not be added. Existing 240-job reporting cadence.
The build also contains prior optional Pico calibration; leave it at 0 for
this isolated diagnostic. This is tooling, not an FPS or gameplay fix.

## Verification and remaining gates

Build: ./build.ps1 -DiagnosticCapture, MSVC x86 /O2, no -Install.
Runtime / remote AMD / headset: NOT RUN. No performance result claimed.
Inspect private build/player-perf/ for DLL/manifest and source-parent receipt.
Compare exact player DLL/profile and preserve rollback before offering a
replacement candidate. Do not package a local INI over the player's working
configuration. No installer refresh or global runtime change was made.
Singularity already offers [Render] TimeOurWork=1. A separate diagnostic-only
branch codex/singularity-player-perf-20261117 now measures its two uncovered
helper calls; no Singularity rendering change or FPS fix is established.

## Context

Read AGENTS.md, BASELINE.md, existing FAILED-APPROACHES entries about capture
slots/wait movement and profiling the wrong scene. Historical local NVIDIA
findings do not establish the remote AMD bottleneck. Full log analysis and
reproducible extraction live in the installer audit branch documentation.
