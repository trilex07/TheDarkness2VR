# Handoff: combined scenes, input, shadows and performance

**Date**: 2026-11-03
**Task ID**: combined-scenes-input-shadow-20261103
**Role**: MAIN
**Branch**: codex/combined-scenes-input-shadow-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: accepted source81bddf2b, DLL E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA
**Integration commit**: pending; no main merge or user candidate acceptance

## Goal
One combined candidate for native scene detection, shadows/reflections, input
conflicts and measured performance, tested in the process-local x86 simulator.
Resolution, accepted stereo, hands/tentacles, FOV and record ownership retained.

## Status
In progress. User explicitly renewed continuation after accidental Escape.

## Changes
- Exact native movie-definition classification: QTE, execution, conversation,
  story. Retained native manager/movie/definition references, bounded names,
  pinned concrete tables/methods. Input protection only; visible asset presence
  is NOT proof of a currently open internal Scaleform popup.
- Native A/B/X/Y retained in protected contexts; wheel, double-X and gestures
  cannot consume those actions. Triangle first press immediately sends Y;
  second short press within350ms requests pause. QTE/execution/story protect
  rapid Y; ordinary conversation still permits the requested double-pause.
  First Y can already perform an action before pause is recognized.
- PresentMon analysis separates swapchains and warns about mixed aggregates.
- Fixed bounded elevated profiler broker: two captures, no game manipulation
  or generic privileged command channel. Its approved window has expired.
- Explicit diagnostic CapturePassStages=3 saves actual C4D1FB10 texture0 input
  and its surface identity for producer correlation. No routine readbacks.
- Explicit VideoDrawProbe=1 observes YUV decode draw targets and compares
  retained surfaces against actual final liveEnd RT0; observation only.

## Evidence
- Production input fixture176 PASS; diagnostic fixture230 PASS.
- PresentMon7 tests PASS; matched-capture validator34 tests PASS.
- Diagnostic build PASS. Latest production rebuild pending.
- Two loss-free ETW captures in private lab/perf. Both are villa/elevator,
  NOT combat. Collector phase labels do not establish gameplay activity.
- Capture2 native chain mean15.464/15.688ms; other chain33.301/33.337ms.
  Swapchain identity inferred, not physical headset FPS or A/B improvement.
  Game runningCPU56.524CPU-s over roughly55s ETL including merge; not proof
  of whole-machine100%. Running samples include driver/game/proxy work.
- Intro PS C1DC2D91 + VS7ACDFC64, depth off, rt != device backbuffer;
  actual final liveEnd RT0 == YUV target. Thus backbuffer-only admission fails.
  World-TV comparison still pending. No arbitrary dimension/timeout filter.
- Shadow depth encoding/producer and reflection semantics remain unproven;
  existing ShadowClipCoordinates candidate stays OFF. Do not report fixed.
- Lean-left/right action names found in QTE scripts; physical binding and
  special avatar types still need native proof. Do not bypass owner guards.

## Recovery / next steps
Private lab: test-output/combined-scenes-input-20261103. Its payload.json owns
exact original backups; tools/simulator-lab.ps1 Update retains them and Restore
checks game closure and protected file hashes. Latest game PID42568 at20:34:29
local, own emulator run. Recheck PID/path/start time before closing it.
Global OpenXR runtime remains SteamVR; simulator override is process-local.
No cache isolation or headset acceptance claimed. Do not add private traces,
shader bytecode, captures, saves, build outputs or local paths reports to public
history accidentally. Baseline remains unchanged.

1. Enter Continue, capture mode3 shadow input, validate Present witness and
   correlate texture/surface with its producer before any lighting patch.
2. Compare world TV YUV output with final liveEnd RT0; replace Bink-lifetime
   cinema classification only after presentation ownership is demonstrated.
3. Finish actual native contexts and input/runtime tests, production build,
   then freeze one candidate plus exact rollback. No new hardware claims.
