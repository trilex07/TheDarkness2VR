# Optimization plan: outdoor combat, fixed image quality

Date: 2026-11-03. Branch: codex/performance-audit-20261103.
Scope: user requests all three directions: CPU threading, RAM/data reuse, GPU
work transfer. Planning only; no runtime, graphics settings or installation
changes. Existing captures only; no request for another recording.

## Outcome and priority

Most promising near-term work is better shader-demand coverage in RAM and a
bounded experiment that prepares immutable right-eye data on a CPU worker.
Moving arbitrary D3D9 calls or AI onto spare cores is not an established route.
GPU offload is a longer-term, shader-specific experiment; spare GPU capacity
alone does not remove serial game/driver work. Do not claim measured savings
from this plan. Keep 2676x2728 source resolution and all current image settings.

Order: R1 shader coverage -> T1 pure replay preparation -> R2 measured data
reuse -> G1 restricted GPU prototype. T0 evidence preparation comes first and
uses the existing traces. T2 native enemy work is conditional on proof of an
independent producer, not a prerequisite to every smaller optimization.

## Evidence used and limits

- docs/research/protected-outdoor-critical-path-20261026.md: outdoor app mean
  20.429ms, render scheduled-running17.925ms, GPU Busy3.818ms. Historical
  protected build, lower source rendering dimensions; combat event loss is
  documented. It supports CPU critical-path investigation, not current exact
  bottleneck proportions or FPS predictions.
- Complete instrumented captures20261030T174801Z-63aeb99c and
  20261030T221816Z-61b77f88 retain sampled CPU stacks, PresentMon and command
  history. The user reported significant collector overhead; do not treat
  those FPS as normal gameplay or remove estimated overhead arithmetically.
- Existing shader hitch:174801 frame19769,90.9187ms producer GetData,
  render WAIT90.878ms, NVIDIA worker35924;88/91 samples include compiler.
  Timing and ownership in pipeline-warmup-runtime-20261030.md. This proves a
  compiler dependency, not an excuse to omit the producer fence.
- Nov2 d3d9.log: same currently installed DLL B97B4477..., marker projection
  was still enabled in that session and is now disabled. Heavy120-frame D3D9
  spans15-17.479ms; these include CPU submission gaps and are NOT GPU Busy.
  Example1920 live10.963/replay5.635ms; CPU replay/history3.00ms and
  right/restore2.06ms are overlapping wall scopes, not independent work.
- Current native VS/PS/binding filters are admitted and skip real calls;
  consumed-record copying, reusable snapshots, constant batching, mode4
  transfer and asynchronous logging are already enabled. Preserve them.
- Read-only inspection on Nov3: shader-demand.bin1,679,264bytes,1024 unique
  codes, exact parse reaches EOF; minimum-two quotas sum2862 objects, compared
  with MaxObjects2048. Quota histogram:2x608,3x155,4x217,5x9,6x4,7x4,8x27.
  These are learned historical requests, NOT proof all2862 are needed in the
  current scene. Legacy shader-preload.bin683 unique codes,1,116,396bytes.
- Nov2 shader counters309requests/196hits/96unknown/17depleted;1024 learned,
  96captureFailures and1852 unused objects. Different inventory snapshots
  need not share one exact instant. Source confirms count cap1024; archive
  is below4MiB, so learning-count saturation is real. Runtime startup ready2048,
  budgetStop1, elapsed438ms; startup pipeline warmup separately1156ms.
- Example recorded frame3600:6382x32-byte commands +158KiB reported payload
  ~366016bytes total. One copy at90Hz is~31.4MiB/s; payload figure is rounded,
  this excludes other copies/engine traffic and is NOT a bandwidth measurement.
  It provides no basis to claim RAM throughput saturation or a benefit from
  allocating gigabytes. Game is x86: address space and COM resource budgets matter.

## T0: Reuse retained traces to select work

Inputs: exact capture DLL/MAP identities, PerfClock render thread IDs, raw
sampled stacks, command-history records and copied INIs. Separate native scene
preparation, mod bookkeeping, driver calls, compiler WAIT, scheduler READY,
and GPU execution. Preserve unknown/unwound portions; inclusive weights must
not be summed. Tag opportunities already implemented since a capture.

Deliverable: exclusive candidate paths with source function, payload ownership,
count/bytes, dependencies and removable work. Do not infer enemy AI cost solely
from more enemies being visible. No further gameplay capture is needed for this
selection. Current millisecond improvement remains unknown until normal play.

## Direction 1: Divide CPU work

### T1. Prepare right-eye constants on one persistent CPU worker (medium priority)

Code: src/stereo_replay.inl ReplayStream, ApplyShiftToUpload,
ApplyVertexStereoUpload, CompensatePsConstants; src/stereo_geometry.h;
src/recorder_sync.h; src/render_command_worker.h.

Current: replay processes an ordered command stream, shader metadata, partial
constant histories, shadow-pass classification, eye-specific transforms and
D3D9 submission together. No safe arbitrary command-level parallelism exists.
BeginReplayRecording locks a stable arena; driver callbacks can invalidate it.
The pure numerical portion is a candidate, not yet isolated or timed separately.

Design experiment:
1. Establish a sealed same-frame command/payload lease and immutable copies of
   required shader metadata, eye parameters, resolved register history and
   shadow classification. The owner performs all COM retention/API observation.
2. Resolve order-dependent histories serially once. Cut independent pure
   transform work into batches; do not enqueue one tiny task per draw/float4.
   Worker reads owned data only and writes disjoint output ranges.
3. Keep D3D9 draws/state/HRESULT handling on the present device owner and retain
   native order. Join before first consumption, not at the next frame. Try to
   overlap only work demonstrably independent of preparation. Scheduling a
   worker and immediately waiting is a serial relocation, not a speedup.
4. Compare existing serial path against worker path at several batch sizes,
   including sealing, copies, history resolution, queue and join costs.
   Small workloads use the serial path. No extra buffered XR frame.
5. Prove no live arena reset/reallocation, metadata mutation or reentrant native
   write can race with a job. Generation changes reject the plan; quiesce/join
   before serial fallback, Reset, fault cleanup or releasing resources.

Offline gates: actual-source fixtures compare every emitted command, register
bank and float bit pattern at each draw; partial/overlap uploads, NaN, shadow,
UI, stateblock, exceptions, Reset and late/failed jobs. Use floating-point/SSE
settings matching the owner. Benchmarks include scheduling and realistic
retained command shapes; synthetic gains are not headset gains.

Stop condition: serial history/driver work dominates or queue/join exceeds saved
owner work. Do not expand to all cores just to increase utilization.

### T2. Native per-enemy preparation (high potential, low readiness)

Native resource/distance/dependency routines already mapped at AC36F0,
BB79A0,C5D850; references in outdoor-cpu-packets-20261026.md. No proven
per-object lifetime, producer/consumer boundary or independent output exists.
First use retained stacks and static analysis to find a pure, expensive producer
with immutable inputs and an output consumed later in the SAME frame. Verify
bytes/callers and cancellation/order/lifetime before a worker prototype.

Do not parallelize AI, animation, native resource collection, scene traversal or
D3D9 mutation on borrowed pointers. No hook or CPU-time saving proposed until
that contract exists. Never reduce enemy count or change gameplay semantics.

### Existing threading is preserved

src/stereo_transfer_worker.h already owns a bounded worker for D3D11 interop.
Mode4 retains D3D9 query ownership on the render thread and an exclusive D3D11
context lease. Latest worker mean~0.15ms, join~0.62ms, overlapping scopes.
An extra transfer worker is low priority and cannot recover the whole CPU
budget. Never share the same immediate context concurrently or simply move
xrEndFrame/BeginTrackedFrame across frames. AsyncLog and archive writers are
already background jobs; shader creation stays on the existing D3D9 owner.

## Direction 2: Use RAM to avoid repeated work

### R1. Demand-aware shader inventory (first implementation candidate)

Code: src/shader_preload_pool.h/.inl; src/startup_shader_warmup.h;
src/pipeline_warmup.h and pipeline_warmup_d3d9.inl.

Aim: learn additional observed shader codes without letting low-demand entries
consume the startup inventory at the expense of repeatedly needed codes.
Separate learned-code count/bytes from precreated COM-object count/time.
Retain legacy file read compatibility and exact bytecode+device identity.

Steps:
1. Replay retained archives/request evidence offline; report known-code coverage,
   depleted demand, selected inventory, bytes and maximum object count. Aggregate
   logs do NOT identify the96 missing codes; if their exact bytes are absent,
   label them unknown rather than manufacture an offline coverage prediction.
2. Introduce bounded learning capacity beyond1024 (candidate4096, byte cap
   retained), plus indexing by hash WITH full-byte equality. Current Find scans
   a vector under mutex: increasing capacity blindly also increases hot lookup
   work. Preallocate/index without persistent engine pointers.
3. Persist demand and recency with a versioned, bounded format; atomic archive
   writes remain off the render thread. Prove old archive migration and corrupt/
   truncated/oversized refusal. Demand priorities cannot starve first-use codes.
4. Select one-use startup inventory within the existing2048-object/10s limits
   initially. Compare fair coverage versus demand-weighted spare allocation;
   raising every quota or creating every known shader is not the first change.
5. Preserve independent shader object ownership, exact HRESULT, native creation
   fallback, device reset/generation and no live refill/driver-thread creation.
   Keep pipeline warmup enabled. Bytecode coverage alone does not guarantee
   all shader/render-state variants are compiled.

Gates: archive round trips, collision/duplicate/index consistency, empty/full
inventory, one-use transfer, Reset and failure tests; startup-time/object/byte
budget tests with retained data. Runtime success later means fewer unknown/
depleted slow creations and reduced hitch tail, NOT promised average90FPS.

### R2. Reuse frame storage and immutable lookup results (after T0/T1)

Already present: g_rec/g_recData keep capacity, arena reserves4MiB,
record_payload_storage.h avoids zero-fill for bytes filled before publication,
consumed/native-owned constant copies avoid general permission syscalls,
constant batch/window can borrow durable arena data. Do not reintroduce these.

Remaining audit: allocations per frame/job, repeated metadata lookup,
partial-history walks and redundant scratch copies. Candidate compact sidecar
indices and prepared constants are bounded per-frame RAM, generation-keyed,
and remain valid only under the exact sealed recording lease. Do not cache
head/hand transforms across frames or native stack pointers across calls.

UI vertex Shadow::Stage has~0.448ms/frame historical leaf sampled exposure,
including necessary work; preserve DISCARD/NOOVERWRITE bytes and restoration.
Do not omit copies on that basis. x86 high-water limits, overflow/fault behavior
and shutdown/Reset reclamation are required. Do not add full buffer mirroring
or large multi-frame queues without measured allocation/copy evidence.

Pass only if total owner critical path incl. cache lookups/copies decreases;
RAM usage increasing is not evidence of improvement. Fast RAM does not execute
CPU tasks; reuse is what can remove work.

## Direction 3: Move suitable calculations to GPU

### G1. Narrow GPU stereo transform prototype (last, research gate)

Current transform is CPU-side adjustment of WVP/STP/PS inverse/projection rows,
followed by ordinary D3D9 shader execution. NIS/compositing already use GPU.
Candidate: exact verified shader families could apply an eye/head correction
from a small per-eye constant in the vertex stage instead of CPU-adjusting each
object matrix. This is a shader rewrite, NOT generic D3D9 multithreading.

Prerequisites: exact shader identity and output semantics, available registers/
instruction capacity, proof of clip-space convention and stereo consistency
for every dependent STP/light/inverse/motion-history path. Unknown, UI, shadow,
lighting, special hand/tentacle shaders remain on existing path. Per-vertex
extra arithmetic may be more work than one per-object CPU matrix operation.

Start offline with captured supported VS bytecode, tiny D3D9 fixture and CPU
reference, no game patch. Compare transformed clip positions, depth and all
other outputs, then budget uploads, shader variants and GPU work. No CPU/GPU
readback is allowed in the frame-critical path just to obtain matrix constants.
A D3D11 compute pass with synchronous readback before D3D9 setters is rejected
as the initial architecture. GPU execution of native AI/resource code has no
established data interface and is not an implementation candidate.

Stop if it leaves the same draw/state call count and saves too little CPU math,
adds costly per-vertex work, compiler variants or synchronization, or cannot
preserve both-eye lighting/geometry. No claim that spare GPU capacity alone
makes this worthwhile. Preserve a single existing backend and rollback switch.

## Acceptance and rollout

Independent branches/switches for R1,T1,R2,G1. Default OFF until actual-source
regressions/build/profile pass; no combined unreviewed experiment. Preserve
resolution/FOV/stereo/hands/tentacles/no-shake/haptics/cinema, query ownership,
ConsumedRecordCopy and accepted DISCARD fix. No engine image patch or culling
bypass. One rollback-protected candidate at a time, MAIN installs only with
closed game when authorized. Later natural gameplay verification can use sparse
existing counters; user does not need to repeat heavy collectors for planning.

For each candidate record: owner critical-path mean/p95/p99 where available,
compile/create hitches separately, memory high-water/startup duration, visual
acceptance and input/head latency. Existing historical traces select targets;
they cannot validate a new binary's FPS. A gain must exceed run-to-run variation
and remain after queue/join/index overhead. Revert a regression, do not stack
changes to conceal it. No promised millisecond gain or stable90Hz at plan stage.
