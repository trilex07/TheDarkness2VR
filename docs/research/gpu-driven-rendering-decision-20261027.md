# GPU-driven rendering and draw batching: eligibility gate

Date: 2026-10-27. Branch `feature/gpu-driven-rendering`, dedicated worktree.
Fetched and verified MAIN integration base:
`63b04ea902f9ee9ea815289457f43f29235ace63` (`origin/codex/outdoor-frame-bottleneck`).
No repository AGENTS.md found; applicable ancestor .codex/AGENTS.md is empty.
Performance/shader skills applied; prior stereo architecture and rejected
constant-recovery findings reviewed. MAIN checkout and installed game untouched.

## Decision

**NO-GO for a runtime batching/instancing/single-pass-stereo prototype on the
available evidence.** Keep the current D3D9 execution and genuine per-eye replay.
There is a concrete small shader-instancing lead, but neither safe eligibility nor
material CPU saving is established. Do not implement a speculative transform cache,
query merge or renderer translation layer. This is an evidence gate, not a proof
that no gameplay geometry could ever be batched.

New result: the retained gameplay frame ALREADY contains indexed instancing,
with instance counts8,8,18. There are also two repeated visible-geometry runs with
WVP-only uploads that could remove at most15 submissions IF missing contracts
were proved. Those runs do not justify the requested architectural rewrite.

## Actual stream classification

Source: private `C:/Users/Trilex/Darkness2VR/test-output/run_43052_frame_003_trace.txt`.
SHA and source size are in `draw-batching-eligibility-20261027.json` together with
all245 draw records and244 adjacent-pair decisions. This is a historic gameplay
frame at source3076x3140; it is NOT the reference B outdoor frame or a representative
scene distribution. Other historical run IDs mentioned in earlier reports were
not available in the inspected source/worktree/perf trace locations. No new capture.

The frame has4744 commands:215 DrawIndexedPrimitive,14 DrawPrimitive,16
DrawPrimitiveUP,0 DrawIndexedPrimitiveUP. Disjoint adjacent-draw observations:

- 17 pairs: same visible geometry signature, including bound streams/indices,
  declaration, frequency and the five available draw arguments. All17 cross
  opaque state uploads. Geometry CONTENT equality remains unproved.
- 55 pairs: different visible geometry signature with the same known shader
  pointers and visible texture/sampler/stage state. This is not material identity
  or full state equivalence; constants, render states and resources can differ.
- 23 pairs: intervening ordering/resource barriers (e.g. target/clear/blit).
- 1 pair: user-pointer geometry, whose actual vertex bytes are unavailable.
- 148 pairs: other/changed visible state. These include unsupported topologies
  or combinations and cannot be admitted from matching material names.

These counts classify observations, not245 proven ineligible game objects.
**Proven safe merge draws:0; actual eligible population:UNKNOWN.** Every draw stays
separate under the available contract. No geometry/order change is authorized by
this result.

Repeated inline setter attempts: render-state87, stream-source577,
vertex-declaration12, sampler88, indices32, texture3: **799 attempts**. Values are
compared only for fully inline arguments; arena offsets are never values. There
are no success HRESULTs, initial state, complete mutations or effective-state
proof, so this is NOT799 removable calls. Stateblock application and UP binding
side effects invalidate the relevant observations. This finding does not replace
MAIN's existing state/constant work or justify a new persistent state cache.

## A: native D3D9 batching

`src/d3d9.cpp`'s EyeDump emits only a[0]..a[4], although Rec has six arguments.
For DIP the omitted a[5] is PrimitiveCount; Rec.n is also omitted. The dump prints
some float constants with seven significant digits, omits uploads above32 vectors,
and does not serialize initial stateblock, vertex declarations, buffer contents,
query Issue/GetData, buffer Lock/Unlock versions or call HRESULTs. The recorder's
op enum itself has no query boundary. This is not a full API replay capture.

Distinct geometry needs compatible state AND proof of contiguous index/vertex
ranges, unchanged base vertex, topology and correct union bounds. Missing DIP
primitive count prevents even the end-index calculation. Combining strips/fans
also needs topology-specific reconstruction; sorting opaque or transparent draws
is not admitted. COM retention keeps a resource alive but does not freeze contents.
Native batching additionally changes immediate return/error and query semantics;
replay-only batching cannot remove original native preparation or RecPush calls.

For repeated geometry,17 same-signature pairs split as follows:
- records2367,2369,2371 (three draws) and2379..2405 step2 (14 draws):15 pairs
  with just OP_SetVSConstF(c0,count4) between draws. Printed WVP values differ.
-2446->2464 also changes viewport, render state and VS/PS constants.
-2684->2689 changes multiple VS ranges and PS data.
The latter two are not transform-only candidates. Never equate identical material
with equivalent draw state or infer index contents from resource pointers.

## B: hardware instancing and real shader evidence

Records249,290,3434 use indexed draws with stream0 INDEXEDDATA|8,|8,|18 respectively,
and stream1 INSTANCEDATA|1. Stream0 stride4, stream1 stride64; frequencies reset
to1 afterward. These are observed attempted native instancing commands, not a
newly available34-call saving. Hashes9E39DAF1 and76CBA109 are recorded; matching
bytecode was not found among the inspected local shader files. No inference that
these are trees, static objects or rigid matrices follows from stride64.

The small repeated-geometry candidate binds VS B7EDA4FD. Local
`DepthPrePass_02_vs.bin` is252 bytes and reproduces that FNV1a hash; its saved ASM
has vs_3_0, position input and c0..c3 WVP transform. The shader index also associates
the same hash with Query.hlsl. A hash match does NOT identify the live pass as an
occlusion query, but it makes ignoring missing query boundaries particularly
unjustified. SHA256 provenance for the private binary/ASM/index is retained in JSON;
no shader bytes or disassembly are committed. This is artifact inspection, not a
live-bytecode capture or independent re-disassembly.

A hypothetical instanced variant would add four float4 matrix inputs to a known
free stream, change the declaration, rewrite WVP use, and preserve original per-eye
transformation of EACH instance. Native indexed frequency support alone cannot
make the unchanged shader consume instance transforms. Must also prove resource
pool/usage, index extent, free input registers/streams, query boundaries, alpha and
depth/stencil behavior, and buffer ownership through both eyes. Animated/skinned
objects need per-instance palette ownership, not merely a world matrix. None of
these absent contracts is supplied by repeated pointer identities.

The two runs would reduce17 submissions to2, i.e.15 fewer, **6.12% of245** in this
historical frame, before additional uploads/binds/shader handling. This is only
an opportunity count conditional on ALL missing contracts. No prototype meets
the substantial-impact gate. Native already-instanced groups must remain intact.

## C: single-pass stereo

D3D9 SM3 exposes instancing frequency, but not a geometry shader or the standard
SV_RenderTargetArrayIndex mechanism for routing instances to eye texture layers.
Rendering the same draw twice in a wrapper remains two submissions. Atlas-based
experiments would still need per-eye viewport/scissor, clipping, depth/stencil,
screen-space sampling, lighting/shadow and UI/history correctness throughout
intermediate passes; current recorded shaders do not provide that contract.

A dedicated D3D11/12 execution path needs shader translation, resource/update
tracking, state translation and per-eye target/constant ownership. D3D11 stereo
instancing uses instance ID plus array-slice routing through GS or supported VPRT.
The existing D3D11 consumer only transfers/composites the D3D9 output; it does not
execute native scene commands. No measured compatibility/coverage result justifies
rebuilding it. D3D9On12's prior gameplay regression is not reopened.

## CPU gate and preferred architecture

Reference B combat (same retained zero-loss ETW used by MAIN), render TID28400,
1141 phase Present rows: inclusive RecPush2273 weights=1.992112 ms;
ReplayStream456=**0.399649 ms**; CD3DBase::DrawIndexedPrimitive319=**0.279579 ms**;
DrawIndexedPrimitiveWithDiffuse326=0.285714 ms. These overlap and are NOT summed.
Values were re-read from retained cpu-phase-samples.json, not newly captured.
Partial ancestry and historic workload prevent treating them as strict caps.

Against the6.43944 ms wall deficit, eliminating the ENTIRE identified0.400 ms
replay exposure would cover only6.2%; eliminating the entire identified0.280 ms
DIP exposure only4.3%. Neither is achievable by the candidate; these are scale
comparisons, not saving predictions. Do not scale outdoor CPU by the15/245 ratio:
these are different captures and heterogeneous draws. No defensible positive
expected CPU saving can be assigned; benefit may be zero/negative. A >=1 ms
architectural benefit is not established. Changing this branch changes runtime
performance by zero because runtime code is untouched.

Preferred architecture stays native D3D9 + recorded same-frame per-eye replay +
existing D3D11 transfer. Existing MAIN-owned constant batching/windows remain
separate experiments; this work neither enables them nor repeats their research.
Single-pass stereo cannot recover a nonexistent second native scene traversal.

## Validation and MAIN integration

PASS:10 new bounded text-auditor regressions (missing arguments, opaque uploads,
unknown initial state, stateblock invalidation, UP side effects, frequency reset,
barriers, malformed records and attempted-setter accounting). Real trace totals,
244-pair partition,799 repeats and8/8/18 instancing reconciled independently.
PASS: existing matrix/provenance suite25/25, including real trace, separate VS/PS
banks and stateblock cases; existing stereo transfer-order5/5.
PASS: x86 actual replay float-loop regression, production AND diagnostic flavors,
each72 streams/432072 records/62893 barrier snapshots. Non-float bodies are
observers, not full driver emulation. PASS: x86 protected head117021 checks.
Private logs: `%TEMP%/d2vr-gpu-driven-regression` (isolated from MAIN fixtures).

NOT RUN: new runtime prototype, production/diagnostic DLL builds, real GPU pixel
or depth/stencil comparisons, full resource-lifetime suite, runtime A/B, PSVR2.
No runtime source changed; inherited fixtures are not acceptance of a new renderer.
The baseline full builds from the earlier stereo architecture investigation are
not claimed as fresh builds here. No new flags, DLLs, profiles or installed edits.

MAIN may cherry-pick this evidence/tool commit alone; no renderer integration or
runtime rollback is required. To reproduce, use the README command on the private
trace. If MAIN later has an independently authorized COMPLETE command/payload
capture with query and resource versions, reconsider ONLY the two identified
runs first and require >=1 ms affected CPU evidence before major implementation.
Do not add a new profiler, launch Steam or collect another trace for this branch.
If coverage remains small, keep the no-go decision.

## Primary API references

- [D3D9 indexed instancing](https://learn.microsoft.com/en-us/windows/win32/direct3d9/efficiently-drawing-multiple-instances-of-geometry): streams, declarations, shader inputs and resetting frequencies; indexed hardware versus nonindexed software distinction.
- [SetStreamSourceFreq](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setstreamsourcefreq): frequency flags and HRESULT contract.
- [HLSL system semantics](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics): array-slice routing and optional VPRT support.
- [D3D stereo rendering](https://learn.microsoft.com/en-us/windows/mixed-reality/develop/native/rendering-in-directx): instance ID and per-eye projection/target selection.
