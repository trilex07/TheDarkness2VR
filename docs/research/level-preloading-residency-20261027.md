# Level preloading and resource residency: measured gate

2026-10-27. SIDE branch `feature/level-preloading-residency`, dedicated worktree,
base MAIN integration `63b04ea902f9ee9ea815289457f43f29235ace63`.
No Steam/game access, deployment, installation or MAIN checkout modification.
No EXE patch, new profiler, automatic merge or speculative runtime hook.

## Decision

**No runtime preloader or residency override is admitted on retained evidence.**
The strongest measured hitch is shader-creation/driver waiting, not archive disk
latency. Its native loading/owner gate is unresolved. Other location-transition
stalls may have different causes: the retained measured window is stationary,
not a marked building-exit transition. Do not claim the user's location problem
has been diagnosed completely or that streaming explains outdoor17.55 ms.

New evidence: bounded disk/hard-fault analysis of the EXISTING ETL, read-only x86
LAA confirmation, and a concrete resource-request boundary in Ghidra. These
results reject blind whole-archive warming as a fix for the measured shader hitch.
They do not reject targeted prefetch for a future independently proved disk stall.

## 1. What caused the measured stall?

Private source: `perf/log-ab-20261026/B/20261026-105632/combat.etl`, PID35388,
render TID36804. Despite the filename, B crashed before combat; use only the
retained first eight seconds after the stationary marker,2526975.4..10526975.4 us.
ETW reports zero lost events/buffers, but the raw manifest remains invalid.
This is not a completed gameplay A/B or a labelled location-transition capture.

Worst frame2879414.4..3009519.8 us lasts130.1054 ms. Rejoining retained blocked
intervals by overlap reproduces shader creation/driver97.865 ms in32 segments,
shared transfer5.539 ms, NVIDIA other0.553 ms, OpenXR0.185 ms, unknown0.287 ms.
These are wait durations, not call counts, and overlap driver-worker execution.
The shader share is about75.2% of this frame, not75.2% of sustained outdoor work.

Existing xperf actions, bounded to those windows, add the following:
- Eight-second game hard faults:19 /495616 bytes in PhysXCore.dll,1 /32768 bytes
  in nvgpucomp32.dll. No game archive hard faults appear in this output. This
  does not mean no buffered reads or no cached asset decompression.
- F.Texture.cache:5 disk reads /327680 bytes, summed disk service1107 us,
  summed IOTime1535 us over the WHOLE eight seconds. Neither sum is render-thread
  critical-path delay or a predicted saving. Cached reads do not need disk I/O.
- Within the130.105 ms hitch, the disk detail report contains only5 ETW collector
  writes and no reads. There is no observed archive disk read to move out of this
  frame. This does not exclude work on data already resident or I/O completed
  before the frame, and does not assign all background effects to the game.

Eight requested source categories:
1. File reads/hard faults: observed above; no proved dominant archive wait here.
2. Decompression: archive format supports it; no measured decompressor critical
   path or per-location amount was established. UNKNOWN, not zero.
3. Texture/buffer creation/upload: possible work, no attributable transition
   total. Prior A41112 buffer Lock is not evidence of new texture creation.
4. Shader creation/NVIDIA waits: directly established dominant hitch component.
5. Synchronous resource loading: native request wrapper found below; no measured
   blocking interval connects that wrapper to this hitch or the next location.
6. Eviction/recreation: no resource-generation/residency timeline proving churn.
7. Native scene initialization: no labelled transition or connected call graph.
8. Sustained outdoor CPU: separately observed15.739 ms RUN/frame; cannot be
   relabelled a load stall or assumed to disappear after preload.

## 2. Approach decisions

### A. External 64-bit OS cache warmer: technically possible, not justified here

The existing local QuickBMS script (`C:/Users/Trilex/Darkness2VR/tools/darkness2.bms`)
uses paired .toc/.cache files. TOC entries are0x60 bytes with64-bit offsets,
compressed/uncompressed sizes and hierarchical IDs. Compressed entries contain
chunks and distinguish Evolution/Oodle decoding paths; this is script evidence,
not a new verification of every archive or a per-location dependency manifest.
No assets were extracted, copied or redistributed.

A separate64-bit process could read explicit compressed archive ranges into the
Windows cache with a small reused buffer, or map bounded ranges and request
PrefetchVirtualMemory. It would avoid large buffers inside the32-bit game, but
would NOT decompress game assets, populate native handles, create textures,
upload GPU resources or compile required shader variants. Cache hints are
reclaimable, not guaranteed residency; scanning everything can evict useful cache
and compete for disk bandwidth/physical RAM. No next-location range list, reliable
lead time or disk-bound transition is available. Therefore no helper is shipped
merely because it is implementable. No whole-install scan or automatic warmer.

A future justified prototype would need a verified exact file/range manifest,
strict byte/time/pressure limits, cancellation, read-only handles, no process
injection and a loading-only MAIN-owned invocation. Test synthetic files first.
Do not schedule it during gameplay or retain whole uncompressed archives.

### B. Native prefetch: request ownership narrowed, loading boundary unresolved

Fresh readOnly/noanalysis Ghidra export of pinned EXE SHA256
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`:
`B61190` reads handle at receiver+0x0C, increments handle+4, then calls the manager
at global010A1CBC through virtual+0x28 (callB611C3). One argument depends on the
byte at [010E1B08]-1; remaining observed arguments are zero. The boolean's meaning
is UNKNOWN; do not name it async/blocking without the concrete manager contract.
This is a retained request dispatch, NOT a verified loading callback or a pure
read-only warmup. Duplicating it can change ownership, priority and release work.

The prior demand cycle remains intact: budgets, reference retention and requests
are already managed. Reused constructorC91330 initializes receiver+44/+48 to zero;
that is initialization, not evidence that the effective runtime budget is zero.
Current runtime receiver, budget writer/value, next-location dependency set,
manager concrete vtable, cancellation/unload and correct dispatch thread remain
unresolved. No guessed vtable hook or extra request was implemented.

### C. More texture residency: no measured budget or pressure contract

EnableTextureStreaming and SpeculativeTextureStreaming have static initial1;
LimitTextureStreaming initial0. These are not effective runtime values. The
existing714B50/5AD6B0 path already budgets requested mip levels and speculation;
609030 resets demand and queues level changes. No safe externally supported budget
setter or measured over-eviction evidence was found. Disabling streaming does not
prove full residency; pinning references defeats unload. Leave all controls intact.

GPU capacity, per-process WDDM budget, native mip budget and32-bit VA are different
constraints. GetAvailableTextureMem is an estimate, not an allocation guarantee.
A separate helper's DXGI usage describes that process, not the game's residency;
current game budget/usage requires correctly attributed in-process or retained
telemetry. None is available for the examined transition hypothesis.

### D. Shader warmup: largest measured opportunity, still blocked on ownership

Reuse the established PS493180 / VS846E70 native getters and selectorCF83D0
contracts from shader-loading-prewarm report, rather than retrying rejected
hash caching or arbitrary variants. Feature words are not physical slot indices.
Early CreateShader failure clears the bytecode slot; invoking binderB4D2B0 also
mutates native state. Newly resolved B61190 is insufficient to identify loaded,
retained owners or safely enumerate future variants. No asynchronous D3D9 calls.

An owner-correct warmup could MOVE first-use creation out of gameplay; it would
not eliminate compilation or prove identical location hitches vanish.97.865 ms
is observed wait exposure, not predicted recoverable time. Required next gate:
verified loading entry/exit and thread, retained owner set, valid cold selectors,
resource/device readiness and reset/unload invalidation for those same owners.

## 3. x86 and memory safety

Read-only private PE matches the captured installed-image hash: machine014C,
characteristics0122, LARGE_ADDRESS_AWARE set. Trace OS pointer size8 confirms
64-bit Windows. Architectural user VA ceiling is4 GiB, ALREADY enabled; no LAA
patch is useful or permitted. Actual available/contiguous VA is lower and unknown.
This ceiling is not4 GiB of free assets and is not a limit on physical VRAM.

The retained manifest has no working-set/commit/VA-free measurements or effective
GPU budget. Targeted log search found no explicit E_OUTOFMEMORY/OUTOFVIDEOMEMORY
marker; absent logging is not evidence of allocation success. Full-residency
working-set size and fragmentation are unknown, so cannot prove either that all
resources fit or that x86 prevents the particular location from fitting. Large
RAM/VRAM alone cannot answer that question. External file cache avoids game VA
for cached archive bytes but not decoded native resources, mappings or staging.

No resource is pinned across unload; no reset path or failure handling is changed.
No GPU readback, synchronous render-thread file I/O or unbounded cache was added.

## 4. Verification, evidence and MAIN procedure

PASS: pinned PE identity/LAA, exact hitch wait overlap,20 game hard faults and
528384 bytes,5 texture archive reads, no disk reads in the hitch detail output,
Ghidra SHA header and EVIDENCE COMPLETE. Provenance hashes and exact intervals
are in `level-preloading-evidence-20261027.json`. Private new exports are in
`%LOCALAPPDATA%/Temp/d2vr-level-preloading`; no ETL/decompilation/asset files in Git.

Reproduction uses existing tools only:
`xperf -i <retained combat.etl> -o <private output> -a hardfault -range 2526975 10526976`;
`-a diskio -summary -range 2526975 10526976`;
`-a diskio -detail -range 2879414 3009520`.
Ghidra CameraEvidence queries:B61190,FF8CE0,85C278,8FC11A; last two return no
analyzed containing function, not invented native loading entries.

Prior evidence read at commits152ecca (native-resource-cycle and native-system-
callchains) and0f9c5f5 (shader-loading-prewarm). Those files reside on their own
branches; no implicit integration/cherry-pick of their code was performed.
No runtime C++/configuration implementation admitted. x86 build/regressions:
NOT RUN because no executable code changed. Runtime A/B and headset acceptance:
NOT RUN. Expected runtime improvement of this documentation-only commit: none.

MAIN integration: consume this report/evidence commit only; no DLL/INI install,
merge or performance switch. Next useful evidence is ONE explicitly authorized,
labelled building-exit/next-location interval using existing capture tools,
with actual memory pressure and resource/loading ownership where available.
Keep the raw manifest/loss verdict; do not infer asset I/O from generic stalls.
If that interval proves archive reads dominate, select exact ranges for bounded
external prefetch. If shader waits dominate, resolve the owner/loading gate first.
If only sustained RUN rises, preloading is not the demonstrated remedy.

Remaining sustained outdoor causes include native execution, unresolved system/
WOW64 work and MAIN-owned submission/recorder costs. Prior ETWP ancestry also
shows observer execution; none of these categories is automatically removable.
This branch does not reopen the closed resource-cycle CPU optimization.

## Primary API references

- [Windows memory limits](https://learn.microsoft.com/en-us/windows/desktop/Memory/memory-limits-for-windows-releases): LAA and architecture-dependent VA limits.
- [PrefetchVirtualMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-prefetchvirtualmemory): cache hint, physical-memory pressure and no guaranteed residency.
- [GetAvailableTextureMem](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-getavailabletexturemem): approximate texture-memory availability.
- [DXGI budget semantics](https://microsoft.github.io/DirectX-Specs/d3d/ResourceHeaps.html): per-process budget/usage and residency pressure.
