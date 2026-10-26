# Native engine -> Windows/driver call chains: bounded decision

Base `f292f2ef89bae4453c557ffee97b48e9f7ffd3ff`; branch
`feature/evolution-native-render-optimization`. Offline investigation on 2026-10-26.
The resource-demand cycle is CLOSED and was not re-investigated. No game, Steam,
XR session, deployment, configuration, runtime C++, or MAIN worktree was modified.

## Decision for MAIN

**ESCALATE: retained evidence does not establish a safe native producer correction
with approximately 1 ms/frame potential saving.** No redundant operation was
proved unnecessary. Do not integrate a native hook/cache from this investigation.
The strongest resolved chain above that scale is native shader-constant submission
through the existing proxy; downstream correction belongs to MAIN. This does not
prove that all remaining cost is downstream: missing WOW64/native ancestry leaves
substantial execution unassigned.

New concrete results are a native vertex-buffer lock, a completion-port work
publisher, and the window-message pump, with CPU versus scheduling separation.
None justifies reconstructing another subsystem or changing its frequency.
The evidence also identifies material ETW stack-collection execution inside the
reported kernel category. Zero event loss does not mean zero observer overhead.

## Source, method and validation

Use reference B, `cpu-candidate-log-ab-20261026T183121Z/B/20261026T184245Z-9997b65a`,
PID43664/TID28400, combat [13223748,33252176) microseconds. ETW stats report
zero lost events and buffers. The raw manifest still has UNKNOWN PresentMon
process-exit status; this report does not upgrade the capture to a full PASS.

A bounded re-read of this EXISTING ETL uses xperf sampled-profile provider
`{ce1dbfb4-137e-4da6-87b0-3f59aa102cbc}`, `-tle -symbols -a dumper`, the range above,
and `-stacktimeshifting`. Local diagnostic symbols and C:\SymCache were used.
The first export was stopped while resolving symbols; only the completed local
symbol export is admitted. There was no new recording or general profiler.

Existing `paths` selected the first recognized MOD frame, not the native caller.
Its UNKNOWN must not be interpreted as absence of native ancestry. This reduction
instead joins adjacent SampledProfile/Stack rows with matching TID and timestamp,
then partitions each non-native leaf by its FIRST native ancestor and first proxy
ancestor. Counts are sampled weights, never operation counts. It additionally
marks stacks containing any `ntoskrnl.exe!Etwp*` frame, once per sample.

PASS: exit0, 4,226,935 input lines, zero stack timestamp mismatches; 17,814 render
sample weights exactly match retained per-module totals: 4,209 native leaves plus
13,605 non-native leaves. Nine weights acquire more specific symbol names in
unrelated modules; module totals and target leaf weights remain equal.

The denominator is 1,141 phase Present rows. These are nominal 1 ms sample weights,
not precise per-call timers or savings bounds. Scheduler RUN15.739 ms, blocked1.500
and ready0.311 ms/frame use 1,140 wholly contained frames, a different measure and
window convention. Do not force sample totals to equal RUN or add overlapping
inclusive subtrees. Raw module costs remain native3.689, kernel4.538, ntdll2.855,
NVIDIA UMD0.845 and system/ambiguous D3D9 0.940 ms/phase Present.

The companion JSON retains the disjoint partition, representative full symbol
chains, exact command, input hashes, wait joins and validation results. Private
extractor and Ghidra exports remain under `%LOCALAPPDATA%/Temp/d2vr-native-system-callchains`;
no native disassembly/decompilation is committed. Saved switch-in stacks are joined
to the PRECEDING scheduler interval via `switchInUs`, not to the outgoing TID.

## Exact callers, operations and contracts

1. **B74720, call B7473E, return B74740 -> SetVertexShaderConstantF -> proxy ->
   VirtualQuery/NtQueryVirtualMemory.** Recovered non-native leaf exposure with
   B74740 as nearest native ancestor is 1724/1141 = **1.510955 ms**. Of this,
   1253/1141 = **1.098160 ms** selects RecPush as nearest proxy ancestor, already
   MAIN-owned; these figures overlap. The pinned Ghidra wrapper reads the device
   at this+0x1A4 and forwards three arguments to vtable+0x178, then ret0xC.
   A representative deeper caller is 71F71F. PS-side return71F736 contributes
   another400 weights (0.350570 ms), 338 through RecPush. This identifies a native
   trigger, not redundant shader values. No values, draw dependencies, state-block
   provenance or invalidation proof in these ETW stacks permits native coalescing.
   Do not duplicate or reopen MAIN's recorder implementation.

2. **A410B0, call A41110, return A41112 -> vertex-buffer Lock ->
   CDriverVertexBuffer::Lock -> DdLockLH -> nvd3dum.** Non-native leaves485 weights
   (**0.425066 ms**), including75 with ETWP ancestry;410 without that marker
   (**0.359334 ms**). The dominant driver PC5C02CB8C alone is227 weights
   (0.198948 ms). Saved scheduler intervals with this nearest native ancestor add
   separate blocked111.476 ms/phase (**0.097700 ms/Present**) and ready45.680 ms
   (**0.040035 ms/Present**). These are not driver CPU and not counts of Lock calls.
   Static code sets this+0x20, uses resource this+0x18, queries vtable+0x28,
   and invokes +0x2C with offset0/size0 and a fresh output pointer. Flags are
   NOSYSLOCK(0x800), plus READONLY for mode1, or DISCARD for mode3. It handles
   null output and applies an alignment offset at this+0x14. Values are verified
   against local SDK d3d9types.h. DISCARD is ALREADY available; forcing it globally
   would violate read/preservation modes. Resource usage/pool, current mode, live
   ranges and unlock/consumer lifetimes are not established. No batching patch.

3. **5220C0, call5222D4, return5222DA -> PostQueuedCompletionStatus ->
   ZwSetIoCompletion.**449 weights (**0.393514 ms**);313 with ETWP ancestry,
   only136 (**0.119194 ms**) without that marker. No attributed blocked interval;
   ready0.752 ms over the whole phase. Ghidra and the PE import at EEB158 agree.
   The producer prepares a payload, copies argument arrays, increments a counter,
   and posts byte-count4 with a payload-specific completion key to a handle at
   this+0x48088, OVERLAPPED=null. A high-counter branch invokes the callback
   directly. A post therefore publishes work; matching API names do not mean
   matching jobs. Consumer ordering, completion counting and payload lifetime are
   unproved. Coalescing wakeups or moving the synchronous threshold is unjustified.

4. **B2E5E0, returns B2E665/B2E6C8 -> PeekMessageA -> overlay/user32/win32k.**
   Together542 weights (**0.475022 ms**), including90 with ETWP ancestry;
  452 without (**0.396144 ms**). Separately blocked110.697 ms/phase
   (0.097017 ms/Present), ready28.873 ms (0.025305 ms/Present).
   Ghidra proves first PeekMessage followed by a draining loop with PM_REMOVE,
   TranslateAccelerator, TranslateMessage and DispatchMessage. The two return PCs
   are initial and subsequent retrieval, not evidence of duplicate processing.
   Some descendants enter overlay library loading/UI callbacks. Dropping messages,
   suppressing the overlay from native code, or throttling the pump has no contract
   proof and falls below the requested prioritization threshold.

These static inspections use the existing RootCause Ghidra project, readOnly and
noanalysis, pinned EXE SHA256
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
For unanalyzed functions, entry bytes were checked after padding/preceding return
and local control-flow inspection; Ghidra recovered data xrefs at101B690 andFBF55C.
Both exports have the correct SHA header and EVIDENCE COMPLETE. Assembly governs
out-pointer/ABI interpretation where the decompiler inferred an awkward local.

## Why the remaining module costs do not admit a native patch

**1460/1141 = 1.279579 ms** of non-native leaves have a kernel ETWP frame in the
sampled stack. This is a disjoint union, not the sum of nested functions. Examples
contain EtwpExpandedStackWalk/EtwpTraceStackWalk beneath unwind/lookup leaves,
including EtwTraceReadyThread during completion publication. This is evidence of
trace-stack execution in the measured thread; it is not a native redundant API.
It is neither a precise trace-off overhead estimate nor a promise that disabling
tracing recovers1.28 ms of production frame time. Do not subtract it from scheduler
RUN as an exact correction. Some monitoring/scheduler descendants may still be
unrecognized by this deliberately narrow marker.

8143/1141 = **7.136722 ms** of non-native leaves have UNKNOWN native ancestry;
this includes proxy-owned and ETWP stacks, not an additional module subtotal.
For the known WOW64 memset category,1705 of1732 weights lack a native ancestor;
27 now expose message-pump descendants. The large unresolved chain terminates in
Wow64SystemServiceEx -> ServiceNoTurbo/BTCpuSimulate -> loader frames. All829
NtQueryVirtualMemory leaf weights similarly lack a native ancestor in these
samples. Loader frames here do not prove repeated loader initialization.
Do not assign missing x86 ancestry to a native allocator, the message pump, or
RecPush by inference. ETW sample weights cannot establish identical arguments,
resource identities, operation counts or safe invalidation.

## Integration and remaining gates

- MAIN should read this report and JSON; the resulting commit is evidence-only
  and can be cherry-picked independently after its base. No runtime install,
  native hooks, INI changes, DLL rebuild or deployment step accompanies it.
- Continue MAIN's already-owned downstream work for the resolved shader-submission
  chain. The native wrapper does not supply evidence for dropping/coalescing calls.
  This report does not mandate a backend rewrite or claim it will meet90Hz.
- Keep A410B0 Lock as the strongest NEW driver-facing native lead, but defer
  implementation unless a future accepted workload shows substantially greater
  exposure AND proves mode/usage/lifetime ownership. Do not spend another cycle
  reconstructing the sub-ms job publisher or resource-demand cycle.
- Unresolved WOW64 ancestry is the explicit escalation boundary. Existing evidence
  cannot name a >=1 ms safe native correction; remaining backend optimization is
  MAIN's engineering decision, not proof all missing CPU belongs to graphics.
- Offline evidence checks: PASS. Runtime C++/configuration implementation: NONE.
  x86 build/regressions: NOT RUN, no executable source changed. Runtime A/B and
  physical headset acceptance: NOT RUN. Performance improvement: NOT CLAIMED.
