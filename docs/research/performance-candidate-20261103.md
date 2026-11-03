# First two performance stages: implementation and candidate

Date: 2026-11-03. MAIN worktree `.main-camera-shake`, branch
`codex/performance-candidate-20261103`, starting source `ba558430`.
Implements R1 and the bounded T1 experiment from
`performance-optimization-plan-20261103.md`. Existing recordings and the retained
shader archive were used; no game launch, fresh collector or resolution change.

## Decision

R1 is ready for a natural-gameplay candidate, not a proven FPS improvement.
T1 is functionally implemented and passes actual-source correctness fixtures,
but **does not pass the performance-benefit gate**. Default test profile is
`Cache`: ShaderDemandCache=1, ReplayPrepareWorker=0. Both switches remain OFF
in the repository profile. Worker/Both are explicit experimental A/B profiles,
not the recommended first play session. No merge to main before acceptance.

## R1: bounded indexed shader demand cache

Files: `src/shader_preload_pool.h/.inl`, startup configuration in `src/d3d9.cpp`.

- Learn up to4096 exact shader codes instead of1024; retain4MiB archive limit,
  8192-word per-code limit,2048 startup shader objects and10s between-call budget.
  A single native driver creation still cannot be preempted by that budget.
- Indexed hash buckets always confirm full bytecode equality and device
  identity. Each creation consumes one independent COM reference; no shader
  aliasing, owner changes or gameplay driver refills are introduced.
- Inventory selects one copy per known code in recency order until the2048
  object budget, then gives spare copies to higher observed demand. With more
  than2048 codes not every known code can receive a startup object. Missing
  inventory falls back to the original creation API.
- Recency is persisted by archive order in the compatible DSW2 format. Legacy
  DSW1/DSW2 files are read; the experimental archive is a separate
  `%LOCALAPPDATA%/Darkness2VR/shader-demand-indexed.bin`. Old demand/preload
  archives remain untouched when this feature is ON. Turning it OFF restores
  the old path and1024-code learning limit.
- Retained private archive:1024 unique codes,1,679,264bytes. Offline inventory
  uses2048 objects with at least one per retained code. It does not reconstruct
  the96 unknown bytecodes absent from the old archive or predict their hit rate.
- Object cleanup releases outside the pool mutex in bounded64-object chunks.
  File IO remains in the existing archive writer; snapshot copying/sorting is
  still on the owner under the pool mutex. This is a remaining overhead to
  evaluate, not a claim that all cache maintenance moved to another core.

## T1: same-frame numeric preparation experiment

Files: `src/replay_prepare_worker.h`, `src/stereo_replay.inl`.

One persistent worker receives owned copies of eligible complete4-row WVP/STP
matrix inputs and eye parameters from the sealed replay stream. The owner
still resolves partial/history/shadow/light decisions, projection publication,
all D3D9 calls, HRESULTs, stateblocks and existing constant batching/reuse.
No native AI, animation, engine traversal or borrowed game pointers are moved.

Worker output is accepted only for the current job/record, exact matrix/parameter
bits and matching floating-point controls. Output publication uses release/
acquire. Not-ready, unknown, partial or changed inputs run serially immediately.
The owner cancels/joins remaining numeric work before ending replay; it never
consumes a prior frame's result. OS scheduling can still delay this join: it is
measured, not assumed free or bounded in wall time. No extra XR frame is queued.

Maximum8192 tasks, scan limit50000 records; minimum256 tasks for admission.
Buffers retain capacity only after completion, reusing RAM between frames.
Each task owns two16-float matrices plus parameters; memory remains a bounded
few MiB, not an unbounded frame queue. Stateblock uncertainty drops shader
knowledge, active matched recording bypasses this experiment, unmasked FP traps
prevent admission, and worker failure disables it for the process.

The actual owner-wrapper ready-output sanity benchmark was approximately
70.6ns serial versus72.4ns cached in one run,41.9ns versus50.5ns in another.
This deliberately optimistic comparison already has every output ready and
excludes prepass/copy/queue/join costs. It uses synthetic matrices, not retained
gameplay timings, and demonstrates no useful owner-work reduction. Variation
and compiler effects mean these figures are not a prediction of gameplay cost.
The additional preparation cannot be called an optimization on this evidence.
Per the plan's stop condition, leave it OFF; do not expand it to more cores.
Full runtime benefit/latency validation is NOT RUN and not required to reject
default admission. A future useful worker needs a larger proven independent
producer or fewer copies, rather than another attempt to relocate tiny math.

Sparse logs: `[PerformanceCandidate]` requested switches; `[ShaderDemandCache]`
learning limit; existing `[ShaderPreload]` hit/unknown/depleted/startup counters;
`[ReplayPrepare]` hits/fallback, prepare average, join average/maximum on first
admission and every300 admitted streams. Scopes overlap other replay work;
do not sum them as independent frame costs. Skipped prepass cost is not included
in admitted-stream averages.

## Offline validation

- Protected-native `/O2` x86 full DLL/MAP/PDB build PASS.
- Worker/actual owner wrapper/actual preparation lifecycle:65589 checks PASS.
  Four rounding modes, changed FP/key/record rejection, NaN/Inf/signed zero,
  projection publication, owned copies, stateblock uncertainty, payload bounds,
  partial uploads, task cap, cancellation, buffer reuse and next-job recovery.
- Candidate cache:12503 checks PASS, including retained private archive,
 4096/1024-code limits,4MiB cap, exact identities, archive validation, priority
  inventory and multi-chunk one-use reference release.
- Existing shader startup/pool regressions42+77 PASS.
- Protected head tracking117021 checks PASS; actual47/130-bone controller
  pack/restore PASS. No headset/culling acceptance is implied.
- Actual replay constant-window/reuse fixtures PASS in both flavors:
 192streams/1,152,192records/114613checks per flavor, exact draw-time banks,
  state order, partial histories, shader changes, stateblock invalidation,
  capture bypass and exception handling. Fixture extraction now stops before
  the separately tested worker lifecycle; its worker is explicitly disabled.
- Actual pixel constant forwarding576 cases per flavor PASS.
- Frozen installer offline fixture PASS: four profiles, exact baseline restore,
  active-game refusal and corrupt candidate refusal. Only private fake files
  are mutated by that fixture.
- Standard build FAIL on pre-existing `ProtectedShotgunSupportOwner` declaration
  without definition in `native_hands.inl`; those files/conditional includes
  are unchanged by this task. Do not use the standard DLL with the accepted
  protected profile. The final candidate is the successful protected-native build.
- Runtime startup, combat frame times, shader-hit improvement, physical stereo/
  hands/tentacles, latency and user acceptance: NOT RUN.

Private detailed build/test logs: worktree `build/*regression.log`,
`build/performance-tests.log`, `build/performance-build.log`.

## Package and test boundary

`tools/prepare-performance-candidate.ps1` freezes the committed source identity,
DLL/MAP/PDB, exact baseline DLL/INI backups, protected file hashes and four INIs.
The preparation guard admits only the two new INI switches: every other setting,
including2676x2728 resolution, remains identical. No installed file is written.

Package `Install.ps1` requires a closed game and exact baseline or last installed
candidate hashes. Default `-Mode Cache`; optional `Worker`, `Both`, `Off`.
`-VerifyOnly` is read-only. `-Restore` restores the exact previous DLL/INI. Every
mode requires a game restart. Installation never launches Steam/the game or
changes EXE, assets, local profile, diagnostic INI, loader or old shader caches.

First test should use Cache alone during the user's ordinary crowded outdoor
combat. Compare startup, visible shader hitches and sustained frame time
separately; do not infer an FPS win from cache coverage. Confirm accepted
stereo/hands/tentacles/camera/haptics/cinema remain intact. No heavy collector
is required to start. Roll back a regression; do not lower resolution.
