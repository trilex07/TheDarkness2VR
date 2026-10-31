# Handoff: stop combat/effect hitches - learn every pipeline state variant

**Date**: 2026-10-31
**Task ID**: pipeline-variant-warmup
**Role**: SIDE (offline only)
**Agent**: Claude (Opus 5.5), session 01FyWRDwj9Pjwkcwz7bewmvq
**Branch**: side/pipeline-variant-warmup (parent 75432ae3; independent of side/present-overlap)
**Baseline**: provisional mode-4 candidate f24ced92; accepted rollback 4a26aabd
**Integration commit**: pending (MAIN)

## Symptom (user)
Steady 90 FPS, drops toward 60 when enemies spawn or effects appear.

## Evidence (existing logs/files only)
- `d3d9.log` PID 26264: every spike window has `[CPUSync] D3D9 producer fence` max
  21.3 / 23.6 / 40.6 / 42.7 / 51.1 ms (normal mean 0.1 ms) and matching boundary work
  25-55 ms. Same windows: `[PipelineWarmup] learned` rises 10 -> 101 and
  `captureFailures` 8 -> 28 during that one session. The producer FLUSH waits for the
  NVIDIA driver thread compiling a pipeline used for the first time (documented
  earlier: nvgpucomp32 on the releasing worker during these waits).
- `pipeline-warmup.bin`: 1530 records for only 538 VS/PS pairs; 365 pairs have 2-10
  state variants (108 pairs >= 5), differing mainly in sampler texture format/type/sRGB,
  then alpha test etc. The driver compiles each variant separately.
- Root cause in code: `ObserveDraw` keyed learning by the VS/PS POINTER PAIR and marked
  it seen after the first capture. Only the first state of each pair per session was
  ever recorded; all other variants (~1000 already observed by chance, more unseen)
  compiled in gameplay every session at their first use - typically new enemies/effects.
  Pairs whose capture failed twice were never retried in any session.

## Change
- `src/pipeline_signature.h` (new, pure): O(1) incremental 64-bit signature of the
  compile-relevant state the Record already stores (RT/depth format, declaration,
  alpha test/func, fog, clip planes, sRGB write, stream frequency, per-sampler texture
  type/format/usage and sRGB).
- `src/pipeline_warmup_d3d9.inl`: setter tracking (one descriptor query per resource,
  bounded cache), learning keyed by signature + VS/PS, 16 captures per frame budget
  (excess deferred, retried, never marked seen), first 24 capture rejections logged
  with their reason (`[PipelineWarmup] capture rejected #n: ...`), save line reports
  keys/deferred. Capture still records ONLY real device state (signature = trigger).
- Hooks (`stereo_replay.inl`, `d3d9.cpp`): Track* after successful native setters,
  never while a native StateBlock is being built. No other behaviour change.
- File format, MaxRecords 4096, startup warmup and rollback compatibility unchanged.

## Expected effect
Each variant compiles at most once per install: on its first appearance it is learned
and the next launch warms it at startup (existing PipelineWarmup path). Repeated
enemy/effect hitches of 20-55 ms disappear after one play-through of the content.
The first-ever appearance of an unseen variant can still hitch once. Steady-state
CPU cost ~0.05-0.1 ms/frame (one set lookup per live draw, one map lookup per
SetTexture/declaration/target change). Startup warmup grows with records
(~0.47 ms/record measured: 1377 -> 641 ms; 4096 -> ~2 s within PipelineWarmupMs).

## Offline tests (this VM: g++ x64, Python 3)
- `test/pipeline_signature_test.cpp`: 2,020,062 checks PASS.
- `tools/test_pipeline_variant_learning.py` 4/4 PASS; existing tools/test_*.py PASS.
- NEW variant/budget case in `test/pipeline_warmup_test.cpp` (actual source, real D3D9
  REF/HAL device; Windows) - NOT RUN here. Harness stubs updated for
  live_record_status and live_resource_binding.
- NOT RUN: build.ps1 / -DiagnosticCapture, Windows runners, Steam/PSVR2.
- Pre-existing: tools/stereo-math-tests/build-sampler-caps-cache.ps1 asserts an older
  exact Hook_SetSamplerState body (already without the StateBlock guard); update its
  expected string if MAIN still runs it.

## MAIN next steps
1. Build both flavours; run test/build_pipeline_warmup.ps1 (+ -Hardware if used),
   build_live_record_status.ps1, build_live_resource_binding.ps1.
2. Play one combat session: expect `learned`/`keys` growth, read the
   `capture rejected` reasons. Second launch: warmup records > 1530, the same
   fights should show producer-fence max < 5 ms and no 60 FPS dips.
3. Rollback: previous DLL; the manifest stays readable by the old build.
