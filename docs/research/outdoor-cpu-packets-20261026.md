# Outdoor CPU work: WOW64 producer and replay packet correction

Status: IMPLEMENTED / OFFLINE VERIFIED. IN-GAME VERIFIED and HMD ACCEPTED:
NOT RUN. No Steam launch, deployment, merge or graphics-setting change.
AsyncLog stays optional/default OFF. The previous outdoor results remain
17.551 ms wall / 15.739 RUN / 1.500 BLOCKED / .311 READY / 3.483 GPU Busy;
6.439 ms wall and 4.629 ms RUN must still be removed for 11.11 ms. No gain is
claimed for the new candidate.

## The previously unknown memset producer

Reused B combat 13.223748..33.252176 s, PID43664/render28400, existing ETL,
local-only matching symbols. xperf SampledProfile extraction: exit0,
4,226,935 lines; 1,732 AVX memset leaf samples. Direct callers: 1,722
Wow64SystemServiceEx, 2 whcbfnDWORD, 1 whcbfnOUTSTRING, 1 whcbfnHkINLPMSG,
6 UNKNOWN. All 18,124 retained stack rows have the same timestamp as their
sample. Nominal sampled exposure ~1.518 ms/frame is not exact wall duration.
Do not add it to inclusive VirtualQuery/RecPush measurements.

Observed WOW64 return is 00007FFBDD898C72 (module RVA8C72). Installed x64
wow64.dll SHA256 83ad5652479de8ea964bd397fc24b6b45040e85924263c0d18a92167bf8c2930,
PE timestamp774DEC9C, SizeOfImage55000. Export Wow64SystemServiceEx RVA8C30:
RVA8C5D clears EDX, RVA8C5F loads R8D=820h, RVA8C65 takes RSP+80h,
RVA8C6D calls RVA360DB; the import thunk resolves to ntdll!memset.
It clears 2,080 bytes of stack-local service context before dispatch. The
recorded return matches this exact call. This establishes WOW64 marshalling
as producer, not vector::resize. System-library image identity inside ETL is
not independently SHA-audited; native callers of individual 64-bit-only
samples remain UNKNOWN. Do not infer all 1,722 calls are VirtualQuery or
patch WOW64, native hashes, syscall stubs or the game image.

Native functions were exported read-only from the D46BB6CB... project using
existing CameraEvidence.java, with EVIDENCE COMPLETE. Measured PCs map to:
AC3809 -> AC36F0 (graphics resource-reference collection), BB79D8 -> BB79A0
(resource/LOD dependency updates), 672DD3 -> 672DD0 (small state accessor),
BF2849 -> BF27A0 (native texture/sampler binding; already caches bound owner),
9F210E -> 9F2030 (resource-list dispatch), C5D8D0 -> C5D850 (bounds/distance
and dependency iteration), 6EE803 -> 6EE720 (D3D vertex-buffer lock path),
B74740 -> B74720 (VS constant interface dispatch), 71F71F -> 71F690 (dirty
semantic upload dispatch). These are descriptions from dataflow, not recovered
symbol names. No externally owned safe replacement/lifetime contract for
native collection/LOD routines is established. No native instructions changed.
The function at5222DA remains unanalysed; its import call must not be labelled
GetDeviceCaps based on unrelated inclusive ancestors.

Private outputs remain outside Git: B/analysis-derived/memset-owner-existing.json
and TEMP/d2vr-outdoor-hot-evidence.txt. No EXE, dump, log or trace is committed.

## Actual C++ correction: adjacent float4 packets

Render/ReplayConstantBatch=0 by default. New src/replay_constant_batch.h is a
bounded 4 KiB replay-local staging buffer, with no page cache, COM retention,
shader reuse, allocation or native patch. src/stereo_replay.inl transforms
EACH ORIGINAL upload first, then combines only ascending exactly adjacent
ranges of the SAME stage within observed hardware caps. Every non-float4
command, stage switch, gap, overlap and invalid range ends the packet. No
holes, unwritten bytes, draws, constants or resource transitions are skipped.
The packet submits at its last original constant command inside the existing
replay exception scope. Negative HRESULT consumes the packet exactly once and
disables further grouping; no automatic driver retry. Capture forces original
upload calls so per-record projection witnesses retain their meaning. Live
left-eye submission, native semantics and frame boundaries are unchanged.

Historical run35124/frame000: 11,231 commands; VS float uploads2541 ->2059,
PS1930 ->1303 (1,109 fewer calls,24.8% of these uploads). This is older
3076x3140 scene evidence, NOT the current outdoor workload and NOT an FPS
measurement. Larger overlap-merging was rejected to retain conservative write
ordering. Current driver-exclusive ~.845 ms and D3D9 ~.940 ms are whole-module
costs, not a promised saving from constants. This cannot justify a90Hz claim.
SmallRecordCopy remains a separate OFF candidate; no settings were combined.

Offline: x86 production and diagnostic DLL build PASS. Packet equivalence
regression: 360,000 input operations /80,190 draw/barrier checks, source-buffer
reuse, invalid ranges, caps, raw NaN/Inf bits, stage switches, overlaps, gaps,
exact-once failure. Existing recorder239, protected gestures227, input20032,
Present ABI/private methods/lifetime/Reset/overlay/error/thread modes, image
guard, seven integration profiles and four PresentMon regressions PASS.
Rollback: ReplayConstantBatch=0; previous installed DLL/INIs were not touched.

Next independent source correction is lazy PS staging: the old replay copies
unmodified PS packets unconditionally, including a missing224-vector bound.
Frame-cap mode/current float are still runtime UNKNOWN; prepare only the
read-only observation for a later authorized session. Do not remove SleepEx,
edit encoded EE.cfg, or count limiter changes as removal of measured RUN.

## Continued implementation: lazy PS payload

After the packet implementation (0f3fe03), removed the unconditional memcpy
from actual OP_SetPSConstF. Unchanged packets now use their owned arena pointer.
The temporary copy is created only for the bytecode-verified C4D1FB10 light
consumer when c0 is present, eye translation is nonzero and count is1..224.
Existing inverse-projection/extents compensation remains per original packet.
This also removes a concrete stack-overrun path: CompensatePsConstants rejects
count>224, but the old fallback copied that oversized payload into adjusted[896]
anyway. Invalid arguments now reach the original API without this extra copy;
no unknown native instructions, shaders or exception filters are bypassed.
This is secondary copy/bounds work, not an explanation of a6.44ms deficit.

Regression extracts and compiles the ACTUAL replay PS case, not a duplicate
algorithm: 288 cases cover original forwarding pointer, no copy for unchanged
packets, exact compensated/light bytes, cap224/225/UINT_MAX, count0, stage start,
source lifetime, batch OFF/ON and one API submission. Both x86 DLL builds PASS.
Packet/head/input/gesture/Present and recorder regressions above remain PASS;
no protected rendering/resource/synchronization code changed in this correction.

## Native execution aggregate and the current safe boundary

Read-only existing Ghidra function lookup covered all2,478 distinct native PCs
in the B combat sample set (4,209 nominal1ms samples). 938 identified functions
account for3,825 samples;384 samples lack an analysed function. No function
entry was invented for a sample PC. Largest exclusive groups, over1,141 raw
phase rows: AC36F0 .16564ms, C5D850 .14023, BB79A0 .11569, BF27A0 .06223,
9F2030 .05171, 6F0190 .05083, 6C74A0 .04207. Remaining unidentified native
execution is.33655ms. These costs are disjoint sampled leaf estimates; do not
add their inclusive descendants or extrapolate them into exact timing.

The native work is distributed; the known resource collector/LOD lists run in
the original game submission, not once per replay eye. No safe external API
has been established that can omit their processing while preserving native
eligibility, dirty-material decisions, animation and resource ownership. Blind
heap-vtable swaps, image patches or skipping draws are not justified. The
current bounded corrections target the public D3D9 command boundary; they do
not claim to remove all native scene traversal or all WOW64 service cost.
Private owner lookup remains TEMP/d2vr-outdoor-native-owners/owners.csv.

The existing audit_camera.py now has optional --frame-cap-pid PID. It first
requires the existing full-file SHA/PE/instruction audit, checks the five cap
code fingerprints on disk/in process, checks the PID image path, opens only
QUERY_LIMITED_INFORMATION|VM_READ, and reads48 bytes twice at010EB068.
Reports FrameCapMode and the float at010EB094; nonfinite floats remain UNKNOWN.
Different/short reads reject the observation. It never starts a process, edits
EE.cfg, writes image memory, or calls SleepEx. Eleven auditor/parser tests PASS;
the real read was NOT RUN because no new game session was authorized.
Configuration fields alone still do not prove an effective pacing branch.

Next blocked gate is comparable in-game timing/driver correctness for the new
packet path and the existing guarded-copy candidate, plus this one read-only
cap observation. These require new Steam authorization. Defaults/rollback are
all performance experiments0; image-preserving stereo/head/Sense/gestures remain
in the preserved approved profile. 90Hz remains UNRESOLVED. Installed files
were not changed during either implementation.
