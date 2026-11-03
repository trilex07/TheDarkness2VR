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

### Follow-up21:20 local
- Checkpoint57e7a455 committed earlier; current follow-up continues on the same
  dedicated branch. No final combined user candidate or main merge yet.
- Input poll now caches its repeated pause observation only within one
  SyncOpenXRGamepad call and exact manager/world key; no cached native pointers.
  Legacy tentacle gestures also yield to native action movies.
- Input fixtures203 production /257 diagnostic PASS; validator42 tests PASS.
  Production and diagnostic builds PASS (latest production preprocessor fix
  still needs a final diagnostic rebuild before freezing candidate).
- Two rejected diagnostic captures proved stateblock Apply re-entered constant
  setters and re-applied left-eye transforms. Capture-only exact-device raw
  restore plus explicit full float-bank restore fixes the diagnostic itself.
  New captures34008/931/9174 and34008/932/13691 pass strict validation, original
  PresentD3D_OK, VS/PS banks byte-identical before/after the INTZ copy.
- INTZ source170A0C80 is the actual depth attachment written by preceding
  native geometry. R32F readback is exact-size/hash checked and tied to source,
  consuming C4D1FB10 draw and explicit state-restoration witness. Old captures
  without that witness must not be used as validated converted-depth evidence.
- Actual-depth algebra at head yaw0/0.25: original median reconstruction error
  about0.117 game units; candidate about0.00026. This includes real viewport
  depth remap0.05..1; not an image/final shadow acceptance or FPS measurement.
- Test-only lab ShadowClipCoordinates=1 now enabled for visual A/B. Production
  default remains OFF. Current own game PID50972 start21:17:47; verify identity
  before closure. Original lab payload rollback remains unchanged.
- Video candidate now requires successful native draw, complete SHA-pinned
  YUV PS + clip VS, depth off/full viewport and SAME-FRAME final live RT identity.
  Resident Bink handle alone cannot select Video2D. No timeout/dimension guess.
  Simulator intro entered mode2 at frame1; title returned mode0 at frame2018.
  This fixes the sticky boot/movie classification witnessed here; actual villa
  TV comparison and final visual validation still pending.
- Diagnostic probe surface refs are released at a new frame and Reset/ResetEx.
  Reflection semantics, system-performance fix, special avatar recognition and
  actual QTE/execution/Darkling/carnival/ride scene tests still need completion.

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

## Follow-up after interrupted turn (2026-11-03)
- Continued own simulator process50972. Entered villa hall with visible NPC and
  static plant shadows; mode remained gameplay. Captures933/frame88431 and
  934/frame90424, head yaw0/0.25, both pass strict matched-frame validation.
  Converted depth bank restoration remains exact. Private fern-wall reprojection
  shows stable background shadow placement in this candidate pair; this is NOT
  an OFF/ON comparison or exhaustive shadow/reflection acceptance.
- Prior elevator OFF/ON reprojection residuals were essentially the same
  (mean0.002641 vs0.002692), a weak fixture without an active shadow caster.
  Do not use its algebra improvement as demonstrated visual improvement.
- Closed an ownership fallback hole: a selected RegionMgr local-player rejection
  now supersedes the optional renderer FOV override in the same renderer/frame,
  clearing stale Jackie aim/poses rather than re-admitting that older override.
  Synthetic ControlAvatar split-owner fixture proves foreign avatar rejection,
  valid old Jackie override, no foreign consume, balanced release including throw.
  This does not identify/support the Darkling/carnival native layouts.
- Cinematic fixture PASS; diagnostic build PASS after ownership change.
- CameraShakeTrace sampling/30-second gate now precedes native reads; expired
  diagnostics no longer keep sampling silently. No material FPS gain claimed.
- Intro presentation filter briefly returned gameplay for2 frames during an
  intro transition (484..486); no arbitrary timeout added. Check transition
  ownership before declaring all intro/video transitions complete.
