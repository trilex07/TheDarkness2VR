# Right-eye UI corruption at a vertex-buffer wrap

SIDE branch: `codex/permanent-right-eye-flicker`. Start: `52e86cd7`, integrating
MAIN `3516641e` and accepted source `5ef2106c`. The installed DLL was verified as
SHA256 `C1022E58F987191E91E3EB200951BE355A71FC7C74A586308A608A06B8BC82E7`.
Existing WVP/STP, PS history, native camera, CPU filters and recorder recovery remain.

## New evidence and reproduced defect

The user's 2026-10-30 F12 series `run_31736_frame_000..019` finally captures the
reported defect. In frame 008, the left-eye pause menu is intact; the right eye
contains fragmented text/geometry and loses the menu panel. Adjacent frames 007
and 009 have the panel in both eyes. Private BMPs/traces remain in the game's
`test-output` directory; no game data is committed.

The affected UI draws use VS `7070C5BC`, stream 0 VB `378189600`, offset 0,
stride 24. Their final triangle-list calls in frame 008 are StartVertex 21576
(10 triangles), 21606 (78 triangles), then 0 (40 triangles). The penultimate
draw ends at byte 524160. Earlier frames advance through the same ring; frame
009 resumes near its beginning. Thus the corrupt frame coincides with a wrap
inside this UI sequence. Its 22 observed UI triangle draws consume 55,008 bytes.

The old replay records a mutable VB handle and StartVertex, not the bytes used
at that draw. A later DISCARD renames/discards the entire buffer, including
high offsets outside the new write. Replaying earlier menu calls against the
current generation consumes unrelated/undefined vertices. This is the source
inconsistency reproduced by the regression. The F12 trace does not record Lock
flags or GetDesc: DISCARD and a 512 KiB capacity are the tested model of the
observed wrap, not directly measured fields in that trace.

Microsoft documents [whole-buffer DISCARD semantics](https://learn.microsoft.com/en-us/windows/win32/direct3d9/performance-optimizations).
The private frame-008 trace SHA256 is
`ef9eba008b99f0ddba8f44b05c7971ee32856d53c76440f9c0084251b7dbb365`;
eye0 `8f51e21beb75c199238dfa8de910cc3fb09c772f53598ba77cec1b0ae174beb3`;
eye1 `92c237b9c203fcffb93465f38dc76b99ebc33ef0f5fd504f1fc86fb11274e775`.

## Correction

While general `ReplayBufferVersions=0`, track native CPU writes to dynamic VBs
without creating/uploading GPU replicas. Copy mapped bytes before native Unlock;
publish validity only on success. DISCARD invalidates every older range. Unknown,
failed, locked, aliased and out-of-bounds data cannot become a replay payload.
Metadata is owned by the exact resource via COM private data, not a pointer cache.

For the observed UI shader's nonindexed DrawPrimitive, validate a single-stream
declaration and ordinary stream frequency, then copy the exact consumed range
into the frame-owned recorder arena. Replay through the existing UP command,
followed by explicit stream-0 restoration (buffer, offset and stride):
[DrawPrimitiveUP clears stream 0](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawprimitiveup).
The original live DrawPrimitive and its HRESULT are retained. Static buffers,
other shaders, indexed geometry and the quarantined general path retain their
previous draw handling. No profile change or geometry suppression.

## Validation and cost

`test/build_replay_buffer_versions.ps1 -UiWrap -SourceRef 52e86cd7`:
expected FAIL at `uiObserved==live`. The actual hooks/interception/replay cases
consume the captured 22-draw wrap sequence against a deterministic native driver
double. The current-source command without SourceRef: PASS, exact per-draw byte
hashes, 55,008 owned bytes, original live draws once, stream restoration, balanced
COM lifetimes, offset handling, readonly writes, unknown-range rejection and
partial/discard/failed-commit validity. Two bounded worker callback probes PASS.

General VB/IB consistency plus eleven callback/deadlock probes PASS. Related
actual-source regressions PASS: live setters, accepted resource bindings,
StateBlock construction/synchronization, UP geometry, projection/FOV history,
binding cache, frame references, native constants, pixel metadata, replay state
and surface receipts, eye color history, protected hands/left aim and tentacles.
Both `build.ps1` and `build.ps1 -DiagnosticCapture` PASS on x86.

No added GPU fences, Flush, readback, GPU replicas or explicit replica uploads.
Cost: CPU mirroring of dynamic-VB writes (64 MiB aggregate size budget), per-UI
state getters, owned UI byte copies and normal UP streaming by the driver.
The captured UI adds 55,008 recorder bytes per affected segment. Runtime CPU/GPU
cost and headset acceptance are NOT RUN; offline checks do not prove FPS or the
elimination of every reported artifact. No Steam deployment or game launch.

Local build artifacts, SHA256:

- `d3d9-right-eye-ui-wrap-production-x86.dll`:
  `92FBDEBCDFC9661657473512BD6A84FD45977454F5F8DCB04A58D401C9E49231`
- `d3d9-right-eye-ui-wrap-diagnostic-x86.dll`:
  `988AD9FB9C92C05A91371438A116716893565A0B52D860A55C5F0F28667E5D7B`

MAIN owns deployment and final PSVR2 pause/HUD/blood checks on this candidate.

## Follow-up: mapped-memory CPU cost

MAIN supplied the existing valid capture at
`C:\Users\Trilex\AppData\Local\Darkness2VR\perf\20261030T121752Z-a99117c2`
for integrated DLL `5372cbd2` (SHA256
`4176B8F2AD4D9C12491D4EE06CF26F2B71C01A168DEFA28FD93162030813865E`).
`cpu-raw-window.json` contains 826 render-thread samples at
`vcruntime140.dll!0x67c361ee` in the 3-4 second window. The short raw stacks
include `73F5B7 -> 7778DD -> 992311 -> 14850252`, `73F5B7 -> A87C1C`, and
`4291DA -> 482BAA -> 5C200C -> 524C66`, above the memcpy PC. MAIN identified
native COM Unlock call sites there. The missing mod frame/FPO prevents assigning
the entire reported 51.327 seconds in vcruntime to Shadow::Stage. MAIN also
observed 108-137 ms frames before ETW; collector overhead alone is insufficient
to explain the slowdown. No additional capture/profiler was started here.

Source inspection does confirm Stage reads every tracked dynamic-VB write,
including non-UI buffers. Native writable mappings can be write-combined;
[Microsoft documents the severe cost of reading such mappings](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map).
This is a memory-type hazard, not proof that this D3D9 driver's actual mappings
were WC: the supplied trace does not record their page protections.

The narrow correction replaces only Stage's mapped-source copy with SSE4.1
MOVNTDQA loads, guarded by runtime CPUID, with CRT fallback. Intel documents
streaming loads for WC memory and the required ordering in its
[optimization manual](https://cdrdv2-public.intel.com/821612/248966-Optimization-Reference-Manual-V1-050.pdf)
and [architecture manual](https://cdrdv2-public.intel.com/874240/325462-090-sdm-vol-1-2abcd-3abcd-4.pdf).
CPU MFENCE before/after the streaming reads orders prior native WC writes and
finishes reads before Unlock/reuse. This adds no GPU synchronization, allocation,
VirtualQuery, native D3D call, GPU replica or change to lock pointers/flags/ranges.
Resource ownership, shadow generations, draw capture and stream restoration
remain as above. All dynamic-VB writes are still mirrored: their bandwidth cost
has not been removed, and speedup on WB mappings/unsupported CPUs is not claimed.

`test/build_mapped_vertex_copy.ps1 -SourceRef 973eafea` compiles the actual old
Stage over VirtualAlloc PAGE_WRITECOMBINE pages. Four 256 KiB captures measured
3.351 ms; the changed actual Stage measured 0.920 ms on the same PC. These are
single bounded CPU fixture observations, not game timing or an FPS claim.
Tests also PASS every source/destination alignment, guarded range ends,
fallback copies and sixteen overwrites of the same WC address. Existing UI-wrap
and both callback regressions PASS; general VB/IB consistency and eleven
callback probes PASS. Both x86 builds PASS, as do live recording/resource
binding, UP geometry, StateBlocks, projection/FOV, hands, tentacles and replay
state receipts. Headset acceptance and runtime performance of this follow-up
remain NOT RUN. No Steam/MAIN modification or deployment.
