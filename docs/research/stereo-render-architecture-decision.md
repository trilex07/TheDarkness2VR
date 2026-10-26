# Stereo architecture decision and MAIN handoff

Date: 2026-10-26 (Europe/Warsaw). SIDE; no Steam/game launch or installation change.
Branch: `research/stereo-render-architecture`.
Verified remote base: `ebecf4ffb13d1d5c6100dcfbc02c4a629d9dc969`.
Remote default HEAD was `main`, `1051c5c6fe998020f7a284b6221136927a6a033b`.
Fetched the requested branch and inspected history/worktrees before creating this
isolated checkout. No repository AGENTS.md exists at this base; the applicable
ancestor `.codex/AGENTS.md` is empty. MAIN's dirty checkout was not edited.

## Decision

**NO-GO for a new architecture rewrite on present evidence. Keep the native
left-eye + same-frame D3D9 right-eye replay + existing transfer architecture.**
The preferred bounded alternative is the already implemented transformed-float
window (`f86d0dc`), after MAIN completes its existing adjacent-batch gate. Do not
create a second competing implementation or enable it by default.

No newly identified change meets both the substantial-impact and correctness
requirements. This branch intentionally changes documentation only. This is not
proof that the engine cannot become faster: the safe public-API opportunities
have a much smaller demonstrated exposure than the 6.439 ms deficit. The dominant
problem remains aggregate render-thread execution in native game preparation,
system-service work and submission; no single safely removable owner for the
remaining several milliseconds has been established.

Native traversal is NOT run twice by this architecture. `ReplayStream` calls
`g_orig` D3D9 methods and mod stereo helpers, not native scene/LOD/material
functions. Thus removing a purported second native traversal cannot save time.

## Cost map and evidence limits

Use the existing B combat trace reported in
[async-log A/B](async-log-ab-outdoor-20261026.md) as the 17.55 ms reference:

- Application interval 17.55055 ms: scheduled RUN 15.73932, BLOCKED about 1.500,
  READY about .311. 90 Hz budget 11.11111 ms; wall deficit 6.43944 ms and RUN
  excess 4.62821 ms. RUN includes kernel/interrupt execution.
- GPU Busy 3.483 ms overlaps CPU work. It is not a CPU budget deduction.
- Native EXE leaf samples: 3.68887 ms per raw phase Present. This includes native
  work beyond scene rendering; it is not a measurement of traversal alone.
- Identified proxy C++ leaves .78703 ms; system/ambiguous D3D9 .94040 ms;
  NVIDIA user modules .84487 ms; kernel ntoskrnl 4.53812 ms and ntdll 2.85539 ms.
  These are disjoint sampled module categories from
  [exclusive module CSV](async-log-ab-exclusive-modules-20261026.csv), not
  disjoint functional stages. In particular kernel/ntdll can be descendants of
  recorder, original rendering or driver calls. Do not assign them all to D3D.
- Recorder RecPush inclusive 1.992 ms and VirtualQuery inclusive 1.830 ms overlap.
  The WOW64 memset producer is service marshalling. It cannot be added to those
  values or attributed to vector zero-fill.
- Replay-inclusive .506 ms is from the DIFFERENT older protected outdoor trace
  ([critical path](protected-outdoor-critical-path-20261026.md), 20.429 ms app
  mean). It is useful scale evidence, NOT a rigorous upper bound for current B:
  partial stacks, sample attribution and workload differ. No current complete
  exclusive replay duration is established here.
- Same B run's boundary API wall scopes: 3.321 ms total boundary, Present .664,
  xrWaitFrame .023, xrEndFrame .639 ms. Nested scopes must not be summed or called
  CPU execution. Prior transfer samples (capture .659, reader .320, XR copy .145
  ms) belong to the older trace, not this 17.55 ms phase.
- NVIDIA workers run in parallel. Removing a worker sample is not automatically
  removing serial render latency. No major measured transfer-growth explanation
  for outdoor slowdown was found in the existing investigation.

Evidence quality: reference A/B traces report zero lost ETW events, but raw
manifests retain UNKNOWN PresentMon process exit; workloads differ, so the A/B
is not a causal AsyncLog result. The newer constant-batch A run lost 3,828,807
ETW events and B never ran; it cannot replace this CPU budget. See
[outdoor packets and final failed A/B](outdoor-cpu-packets-20261026.md).
No ETW collection/extraction or memset investigation was repeated here.

## What is actually recorded and duplicated

Source anchors refer to unchanged base source:
`src/stereo_replay.inl:365-550`, `:1131-1495`, `:1577-1888`;
`src/d3d9.cpp:3189-3227`; `src/small_record_copy.h`;
`src/frame_resource_refs.h` and `src/replay_constant_batch.h`.

1. Every recorded draw has one 32-byte Rec (op, n, six arguments), not a copy of
   all device state. Setters are separate records. Constants, matrices, viewport,
   clip/scissor data and UP geometry are copied into the frame arena. Ordinary
   VB/IB draws retain bindings, not whole meshes or native object graphs.
2. Frame start captures a D3DSBT_ALL stateblock plus RT/depth references and caps.
   Before right-eye replay the live-end state and pixels are saved; frame-start
   state is applied, right-eye commands execute, then live-end state/pixels are
   restored. RT application precedes stateblock application to restore viewport.
   Eliminating these snapshots without complete state ownership changes semantics.
3. Shader constants are recorded BEFORE live-eye transforms. Replay transforms
   original uploads for the right eye using shader/pass context. Identical raw
   bytes are not sufficient to eliminate a transformed upload across contexts.
4. Resource bindings retain COM references: RT/DS, textures, streams, indices,
   declarations, VS/PS, ColorFill, stateblocks and both StretchRect surfaces.
   The existing unique-reference mode keeps exact interface identities alive
   once per frame. It does not freeze dynamic resource contents or stateblock
   contents. Retention cannot be replaced by borrowed pointers.
5. UpdateSurface/UpdateTexture records are explicitly ignored by replay: content
   updates have already executed in the live pass. Their recording can still
   occur; removing these no-op records alone has no measured material exposure.
   Do not generalize this exemption to clears, StretchRect, shadows or draws.
6. RecPush resizes a reused arena and validates each source range afresh. RecEmit
   and RecPush each acquire the record lock. Capacity is retained (200,000 Rec =
   6.4 MB reserved, plus at least 4 MiB arena); growth above reserve can allocate.
   The reference vector can grow; the opt-in unordered_set allocates nodes.
   This is not evidence of multi-ms allocation cost: prior vector-resize leaf
   .029 ms, recorder lock .243 ms, COM .178 ms are OLD-trace scale observations.
7. `RecOnRenderThread` is defined but has no callers. Its comment about dropping
   worker uploads is not a proved single-writer contract. Current update hooks
   check g_recActive and emit records. Lock removal or parallel preprocessing is
   not admitted from that comment. Atomic record emission alone would not prove
   total ordering of original driver calls across producers.
8. Most PS payloads already avoid an unnecessary temporary copy (`22cc2d0`).
   Existing windows transform original uploads then coalesce only float-only
   sequences, flushing on every non-float command. This is the already available
   safe separation of eye-dependent computation and reducible driver submission.

## Ranked alternatives

### 1. Bounded state-aware float replay: preferred, already implemented

Keep `ReplayConstantBatch`/`ReplayConstantWindow` isolated and default OFF.
Historical run35124 command ranges: 4,471 uploads -> 3,362 adjacent -> 2,523
windowed (24.8% / 43.6% fewer calls). The old trace lacks payloads and is not
current outdoor gameplay. It proves call opportunity only; existing synthetic
actual-source tests establish bounded register/barrier equivalence separately.

Expected CPU saving: UNKNOWN, possibly zero/negative after bookkeeping. For
planning, a sub-ms target is more credible than several ms; no positive lower
bound is measured. Current whole-module D3D9+NVIDIA leaves total about 1.785 ms,
but include original rendering and other APIs and are not all removable replay
cost. Nor is that sum a strict cap on all downstream kernel cost. A 5 ms saving
has no supporting attribution. The older .506 ms replay scale reinforces caution.

Complexity/maintenance: low incremental, implementation/tests already in base.
Stereo: original per-eye transforms and command order retained; no draw removed.
Compatibility: existing D3D9Ex path, no new device or native hook. Memory: existing
4 KiB adjacent storage plus bounded window banks (about 8 KiB class storage),
reused under the replay lock. Regression risk: driver failure timing and deferred
uploads need live acceptance; tests are not driver-image proof. Preserves accepted
architecture. MAIN's ongoing adjacent A/B remains first; do not substitute an
untested combined profile.

### 2. Compact recorder / preprocessed state stream: defer

Variable-length records, direct decoded handlers, frame-local object IDs or a
single combined append could reduce memory traffic/dispatch. They still must
copy transient source data and retain resources. A second preprocessing pass
costs CPU and may duplicate arena/record storage; sorting is forbidden by draw,
transparency and resource ordering. Current records are only 32 bytes and reused.
No measured decoder/record-padding bottleneck supports a major rewrite.

The large target is the 1.992 ms inclusive recorder, not record width. Even an
impossible removal of ALL that exposure would cover only about 31% of the
6.439 ms deficit (not a predicted wall saving). Fresh range checks dominate the
observed subtree; page caching or SEH-only memcpy lacks a valid lifetime/guard
contract. Owned buffers could use known-safe copies only after establishing
producer ownership; most engine uploads are external. SmallRecordCopy already
provides the bounded experimental alternative and has no confirmed runtime gain.

A persistent replay state cache adds further invalidation obligations: stateblock
Apply, RT/viewport, FVF/declaration coupling, UP draws, draw-scoped tentative
state changes, resets, unknown setters and failed HRESULTs. Start unknown; mark
known only after successful calls; never suppress required error behavior.
No measured count/cost of redundant non-float setters warrants that complexity.
Memory could shrink records but grow state tables; CPU benefit UNKNOWN, no
supported multi-ms forecast. Maintenance/risk medium-high; nominally preserves
architecture only with complete ownership proof. Not selected.

### 3. Eye-independent pass reuse or new submission architecture: no-go now

Shadow-map/pass reuse might remove draws, but the current pipeline reuses mutable
surfaces and has UI-only right-eye history plus per-eye lighting compensation.
A shadow-pass classifier is not proof that all consumers, clears, resource
contents and temporal effects are eye-independent. No complete resource/version
and pass-dependency proof exists. Savings UNKNOWN, within affected replay work
rather than native traversal; high regression risk, potential extra render-target
memory, substantial maintenance. Do not implement a guessed skip list.

Immediate dual-eye draw mirroring could avoid deferred source copies but requires
independent RT/depth/viewport and intermediate resource state for both eyes.
Switching targets/constants twice per draw can add more driver calls than it
removes. It does not remove native preparation and changes pass/history behavior.
D3D11 multiview/translation would require a new shader/resource/state translation
layer: the existing D3D11 device is a transfer/composite consumer, not an engine
render replacement. Benefit is unmeasured; complexity/compatibility risk very
high; additional eye resources/translation caches increase memory. Neither is a
minimal reversible correction to this accepted architecture.

Parallel D3D9 replay is rejected: factory hooks forward native flags unchanged;
there is no demonstrated actual device threading contract, disjoint target
ownership or safe concurrent order. Enabling a threading flag alone would not
make interleaved writes to a single device correct or faster.

## Native rendering conclusion

Existing Ghidra outputs were reused, not regenerated: private TEMP
`d2vr-outdoor-hot-evidence.txt` has the pinned EXE hash and EVIDENCE COMPLETE;
`d2vr-outdoor-native-owners/owners.csv` exists. The committed packet investigation
maps 2,478 distinct PCs into 938 functions. Largest reported native exclusive
group AC36F0 is about .166 ms; BF27A0 already shadows bound texture/sampler owner.
Resource/LOD/dependency collection and dirty semantic dispatch are distributed.
No safe external contract permits skipping them; replay does not call them again.
TheDarknessIIDecompiled is documented in the existing toolchain evaluation as
Lua/Flash scripts, not recovered native renderer source. It supplies no missing
native ownership/ABI gate. No game code/assets were copied into this branch.

The remaining independently larger opportunity is a demonstrated reduction of
native/system-service submission work or shader-creation tail stalls. It is NOT
currently independently implementable under the no-guessed-hooks restriction.
Known shader prewarm ownership/loading gates remain unresolved. This assignment
found no actionable substantial optimization left unimplemented; it does not
relabel those blocked opportunities as impossible.

## Validation performed in this SIDE checkout

PASS: x86 production `build.ps1`; x86 diagnostic `build.ps1 -DiagnosticCapture`.
Initial sandboxed build did not complete; both host-side reruns succeeded. No
`-Install` invocation. These are baseline builds, not a new optimized binary.

PASS, existing regression scripts:
- frame-boundary build.ps1: 2,327,551 assertions; 50,000 synthetic traces and
  25,000 timing scenarios, zero failures.
- build-replay-constant-window.ps1: BOTH flavors, each 72 streams / 432,072
  records / 62,893 barrier snapshots; admission, original float cases, raw bits,
  failures, forwarding receipts, SEH/no retry. Non-float bodies are observers
  and stereo-helper inputs are controlled stubs, not a full driver emulator.
- build-replay-constant-batch.ps1: 80,190 checks / 360,000 operations.
- build-replay-ps-payload.ps1: 576 cases per flavor, including actual PS case.
- build-small-record-copy.ps1: 239 byte/range checks plus admission/config checks.
- build-protected-head.ps1: 117,021 checks; build-rendered-fov.ps1: 1,206 rays
  plus output bounds. These separately exercise existing stereo math.
- Python test_stereo_transfer_order.py: 5; test_present_readiness.py: 8.

Private logs: TEMP/d2vr-side-architecture-tests and
TEMP/d2vr-stereo-architecture-{production,diagnostic}.log. Tests used a dedicated
TEMP subtree to avoid sharing fixture outputs with MAIN. Generated binaries are
not committed; tracked frame-boundary executable was restored after the test.

NOT RUN: full captured-payload stream equivalence (available historic trace is
payload-free), real D3D9 driver/image equivalence, new Steam A/B, PSVR2 acceptance.
Inherited regression PASS does not certify all stateblock/dynamic-resource
semantics, all game scenes, or any FPS improvement. No new C++ prototype exists.

## MAIN handoff and minimal future A/B

Files changed: this report only. Commit: use the commit containing this report
(`git log -1 --format=%H -- docs/research/stereo-render-architecture-decision.md`);
exact pushed SHA is supplied in the chat handoff. No cherry-pick of renderer code
is needed: `0f3fe03`, `22cc2d0`, `f86d0dc` are already ancestors of the base.
Expected new branch performance delta: exactly none (documentation only).

No new runtime flags. Recommended architecture candidate remains existing
Render/ReplayConstantWindow, dependent on ReplayConstantBatch; default 0/0.
Rollback of the candidate: Window=0 and Batch=0, restart; restore the verified
baseline DLL/INIs if a later integration deployed a binary. This report itself
needs no runtime rollback.

For a FUTURE MAIN-authorized free-session test only:
1. Finish MAIN's pending adjacent comparison first: one frozen diagnostic DLL,
   same scene/profile, A Batch=0/Window=0 versus B Batch=1/Window=0. Preserve all
   unrelated performance switches; do not combine experiments.
2. Only if that gate warrants proceeding, isolate windowing with A Batch=1 /
   Window=0 versus B Batch=1 / Window=1; match capture must be OFF because it
   deliberately bypasses batching. Keep existing protected Present/stereo/head/
   controller/gesture settings, FOV and actual source size identical.
3. Use existing collector, not another profiler. Backup/hash all affected files,
   validate EXE identity, finish UAC before Steam, confirm visible gameplay once,
   then existing stationary/combat cues. Inspect completed manifest/loss before
   interpreting RUN/BLOCKED/READY, app intervals and in-phase ReplayUploads.
   Failed calls/exceptions, missing admission or ETW loss reject a full CPU A/B.
4. Human PSVR2 acceptance must cover true depth/both eyes, head motion, hands/
   controllers/gestures, light volumes/shadows/transparency, HUD/pause/UI history,
   transitions/reset and stability. Restore baseline on defects; no automatic
   retry or promotion. Offline call reduction is not an FPS result.

## API constraints checked against primary documentation

- [DrawPrimitiveUP](https://learn.microsoft.com/en-us/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-drawprimitiveup): stream 0 becomes NULL; source data is consumed before return.
- [DrawIndexedPrimitiveUP](https://github.com/MicrosoftDocs/sdk-api/blob/docs/sdk-api-src/content/d3d9helper/nf-d3d9helper-idirect3ddevice9-drawindexedprimitiveup.md): stream 0 and index binding become NULL.
- [SetRenderTarget](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget): viewport side effect.
- [D3D9 threading](https://learn.microsoft.com/en-us/windows/win32/direct3d9/multithreading-issues): reset/lifecycle thread restrictions and deadlock risks.
- [API profiling](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/direct3d9/accurately-profiling-direct3d-api-calls.md): driver state work can be deferred to draws; call-count reduction alone does not determine time saved.
