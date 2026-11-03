# Handoff: Existing-capture performance audit

**Date**: 2026-11-03
**Task ID**: performance-audit-20261103
**Role**: MAIN (session explicitly designated MAIN; bootstrap defaults SIDE on task branch)
**Branch**: codex/performance-audit-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: installed B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2; marker projection OFF, HUD asset hidden.
**Integration commit**: none

## Goal / status
Analyze occasional drops, especially many enemies in outdoor areas. User explicitly
requests reuse of existing captures, no new profiling session. Analysis complete;
no renderer/config/Steam modifications, game not launched, no FPS gain claimed.

## Context loaded
BASELINE.md; FAILED-APPROACHES.md; performance-optimization skill; protected
outdoor critical path; outdoor CPU packets; current CPU engine opportunity;
consolidated native CPU and pipeline-warmup runtime investigations.

## Evidence and conclusions

1. Latest log (last write Nov2 19:30; predates marker hiding, same DLL SHA)
retained privately in test-output/performance-audit-20261103 with current INIs
and identities.json. Log SHA2bf04cbbf715e01be3f0b3b61921de0be2a45ce59f1ab0de12af2a4c335d4b17.
At frame1920: D3D9 timestamp span16.599ms (live10.963/replay5.635);
frame2040:15.676ms (10.296/5.381). Several 120-frame windows mean15-17.479ms.
Timestamp spans include CPU submission gaps, NOT GPU Busy. XR world-layer cadence
38.8-52/s in late gameplay versus physical90Hz; runtime predicted periods often
22.222ms (occasionally33.333), not proof of physical display frequency or fixed
application limiter. At90Hz budget11.111ms. Current source2676x2728 per eye.
NIS sample~0.13ms, D3D11 reader mean~0.010ms; these do not explain sustained
large deficit. CPU stereo replay/history~1.6-3ms, right/restore~1.7-2ms in shown
windows, includes driver waits, overlaps other timings; never sum all scopes.
Extremes191.559ms D3D9 /3077.739ms Present include transitions/loading/pause;
not attributed to enemy AI without a matched timeline.

2. Protected outdoor capture20261026T162734Z-f0a7a06e: outdoor mean20.429ms,
GPU Busy3.818ms, scheduled render RUN17.925ms; indoor14.178ms /2.902ms /
12.545ms RUN. Predominantly execution before GPU submission in THAT build,
not ordinary fence blocking. Combat ETW lost1,026,324 events; retained outdoor
intervals/supporting source boundaries described in protected-outdoor report.
This is historical CPU diagnosis, not current exact distribution. No forced
ZoneView/visibility/native dependency routine changes justified.

3. Re-read complete zero-loss20261030T174801Z-63aeb99c and221816Z-61b77f88
raw sampled JSON: exit0/partialFalse. Render tids verified from their own
PerfClock (11016/7828). Respect instrumented results: user reported collector
cost~20FPS. In later capture combat1528rows mean16.363ms; GPU Busy5.957ms;
not physical headset FPS. Prior call exposures1.408ms float setters +0.420ms
bindings include necessary work. Current log NativeSubmission enabled1 denied0,
VS/PS/binding skips nonzero, so old recommendation to fix admission is already
implemented. ConsumedRecordCopy and AsyncLog also active. Do not sell these
as new optimizations or extrapolate old sampled weights to current FPS.

4. Same174801 command-history/runtime report proves one90.9187ms producer
GetData(FLUSH) hitch, render WAIT90.878ms, readied by NVIDIA worker35924;
88/91 worker samples include nvgpucomp32.dll. This is compilation dependency,
not proof of all fence waits/CPU work. Preserve required fences and COM ownership.
Current startup warmup4096records/8192draws, success/restored/driverComplete,
1156ms. It is already enabled; not exhaustive pipeline coverage.

5. Concrete remaining coverage issue in current shader pool: log ready2048,
entries1024,budgetStop1; later requests309 hits196 unknown96 depleted17,
captureFailures96, pending1852. src/shader_preload_pool.h MaxShaders1024,
MaxObjects2048; Remember rejects at capacity. Current shader-demand.bin is
1,679,264bytes, below4MiB archive bound, so count saturation is established.
Do not simply increase all precreated copies: 1852 unused objects coexist with
misses, and fixed startup object budget can still exclude newly learned codes.
Candidate direction: bounded demand-aware learning/admission covering observed
unknown codes, prioritizing actual demand within existing startup object/time
budget; preserve exact bytecode/device, independent one-use COM ownership,
legacy archive compatibility, reset/failure fallback. This targets hitches,
not guaranteed sustained outdoor CPU improvement. No candidate built/deployed.

## Recommended next work (no new recordings required to start)
1. Offline shader-demand coverage improvement using retained archives and
creation-history fixtures; quantify coverage, memory, startup budget (not FPS).
2. For sustained combat, use existing replay/native call traces to identify
remaining duplicate work outside already admitted float/binding filters.
UI vertex Stage has684 leaf sample weights/1528frames (~0.448ms sampled
exposure) in221816, includes required wrap-safe copies and driver cost. Preserve
accepted right-eye DISCARD correction; exposure is not removable saving.
Native per-enemy preparation/resource traversal has no proven replaceable
ownership contract. Do not reduce AI/enemy count or bypass culling validity.
3. Resolution A/B is a secondary diagnostic only if actual GPU Busy dominates;
older outdoor evidence does not justify lowering clarity as first solution.
EngineLODScale already1.25; no blind increase or FPS guarantee.

## Validation
Collector preflight PASS; no recorder started (user asked existing data only).
Raw captures/logs/archives remain private; installation unchanged. No tests
needed for documentation-only audit. git diff --check required before commit.
