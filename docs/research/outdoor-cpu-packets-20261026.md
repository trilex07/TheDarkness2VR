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

## Continued implementation: measured forwarding receipt and float-only windows

The 22cc2d0 adjacent-packet candidate lacked actual forwarded-call counts. The
new diagnostic path counts entry into the original D3D9 VS/PS float setters,
returned failures, submitted float4 vectors and replay exceptions. Logical
records are counted separately. Counters are local/in-memory and emitted with
the existing 120-pair StereoPerf report, using its existing first/last QPCs and
frequency. No new per-frame file I/O, timer, tracing provider or profiler was
added. Production compiles the receipt out. Counts explicitly exclude draw-scope
setters (tentacle save/restore); those calls are unchanged in both arms. A
counter-window crossing a phase boundary is excluded from phase comparisons.
Any failed call/exception is a correctness failure, not an upload reduction.

The requested adjacent A/B still changes ONLY ReplayConstantBatch=0/1, uses one
new diagnostic DLL and preserves the 22cc2d0 lazy PS bounds/copy correction in
both arms. AsyncLog, SmallRecordCopy, SamplerCapsCache, LastConsumerReaderFence
and the separate new ReplayConstantWindow are OFF in both. Protected Present,
head/input/gestures, resolution, stereo and lighting settings are inherited
unchanged from the frozen previous A profile. Win32 GetPrivateProfileIntW and
full effective-key comparison validate the two prepared profiles. Nothing was
deployed. Existing Capture-DarknessCombat.ps1 remains the only collector: UAC
before Steam, one initial visible-VR readiness check per launch, then audio-only
10s stationary/20s combat phases with no feedback questions during capture.
Include the already implemented read-only frame-cap audit during an authorized
run. No native limit, encoded config or image byte is modified.

Next isolated implementation: Render.ReplayConstantWindow=0 by default, also
requires ReplayConstantBatch=1 and is forced OFF for matched capture. It retains
final transformed values for each independent VS/PS register bank ONLY within
an uninterrupted sequence of float-constant commands. Every other command,
including draws, shader/state/resource changes, integer/bool constants, RT/VP,
state-block application and unknown operations, flushes both banks. Dirty runs
submit only registers actually written; unknown holes are never synthesized.
Overwritten values can be combined, because no observer or draw occurs between
them. Each original upload is transformed before storage, so partial matrices,
projection metadata and shader-dependent processing keep original semantics.
This is not a persistent device-state cache and does not skip any draw. Owned
storage is reset per replay; empty flushes are O(1), nonempty scans are bounded
to written spans. Invalid/null/unknown-cap calls flush then retain original API
arguments. A failure consumes each pending run once and disables subsequent
deferral; the existing replay exception handler aborts pending state without a
retry. No new exception suppression was introduced. Rollback is Window=0.

On the SAME older run_35124 trace's command ranges (not the current outdoor
scene), original4471 VS/PS calls become3362 with adjacent batching and2523 with
the float-only window, reductions24.80% and43.57%. The window's2523 is obtained
WITHOUT crossing any state setter. This demonstrates extra call opportunity,
not a measured CPU saving or rendered-image proof: this private trace lacks
constant payloads. Current outdoor D3D9/NVIDIA whole-module exclusive estimates
(.940/.845ms) include other APIs and are NOT eliminable constant-only costs.
Native resource/LOD work remains distributed among938 functions, largest
identified exclusive .166ms. No safe external interception is established for
omitting their traversal. Source-page permission caching remains rejected:
1.830ms VirtualQuery exposure overlaps the1.992ms RecPush subtree; it cannot be
added to it, and external buffers have no proven stable lifetime/protection.
The completed WOW64 memset-producer analysis was not repeated.

Offline regressions compile actual ReplayStream admission, driver lambda, VS/PS
cases, lookahead, SEH and receipt aggregation; only non-float bodies become
bank observers, while stereo helper inputs are controlled test stubs. Both x86
build modes pass72 streams/432072 records/62893 snapshots each, including actual
4->2 driver entries for interleaved VS/PS, all-register bit identity (NaN/raw
bits), caps0/32/96/224/256, overlapping/descending ranges, stage interleaving,
unknown barriers, input ownership, default OFF, capture bypass, HRESULT failure
and one-entry SEH/no retry. The existing real stereo-math tests separately
exercise the unmodified transform helpers. Actual PS case576 checks per build
and existing adjacent regression80190 checks/360000 inputs pass. Protected
Present ABI/private methods/refcount/Reset/overlay modes, image guard, head117021,
input20032, gestures227, source-copy239, FOV1206, composite171 and material424
regressions PASS; auditor11, integration profiles7+3 and PM analyzer4 PASS.
Production and diagnostic x86 builds PASS; frozen separately with matching
PDB/MAP. No new Steam/HMD result exists. Runtime saving is UNKNOWN, and the
previous outdoor17.55055ms mean/15.73932ms RUN leaves6.43944ms wall/4.62821ms
RUN above11.11111ms. Neither offline call counts nor XR layers/s establish90Hz.

Gate: authorize two controlled Steam launches for the adjacent A/B only after
checking no other game is active, fresh EXE SHA/PE/bytes, backup all DLL/INI
state. If ineffective, keep Batch=0; no automatic retries or promotion of the
window variant. Native FrameCapMode/MaxFrameRate observation remains read-only.
The window candidate remains IMPLEMENTED/OFFLINE VERIFIED, not IN-GAME VERIFIED
or HMD ACCEPTED. No merge, deployment or automatic game launch.

## Authorized adjacent A/B: A retained, B not started (2026-10-26 21:25 UTC)

Exact f86d0dc diagnostic DLL E6B77B83977E8B93B4DD36177A4C1085B7CA86D2F70EBD9A93234727D98D7C6D
was deployed after a fresh original EXE audit, backup of all five DLL/INI files,
and UAC before Steam. All eleven frozen package hashes and effective A/B keys
were checked. A had Batch=0, Window=0, SmallRecordCopy=0, AsyncLog=0; no other
performance experiment was enabled. No native EXE/FOV/resolution patch was
admitted. User confirmed visible VR outdoors and closed the game manually after
the five end cues. Do not report that normal exit as a crash.

Both scheduled phases completed. Zero-loss PresentMon retained 540 wholly
contained stationary intervals and1080 combat intervals. Stationary mean/p50/
p95/p99/worst were18.54664/19.04000/22.45642/23.56075/28.64580ms; combat values
18.53983/18.71010/22.22909/24.56798/106.31170ms. Combat frames above11.11/22.22/
33.33ms were1063/55/4. Combat GPU Busy mean was3.03491ms; Present interval rate
53.905/s is NOT proof of unique headset display frames or90Hz. PresentMon's
18.02817ms CPUBusy is not sampled render-thread execution or a wait-stack total.

Actual original setter-entry receipts wholly inside each phase: stationary
480 replay streams, VS1000017/PS744142 logical records and identical actual
calls; combat960 streams, VS1961620/PS1379146 records and identical actual calls.
All receipt HRESULT-failure/exception counters were zero. Last periodic native
Present witness had13000 entries/forwards/returns/D3D_OK, zero recursion/errors/
statuses/fault, and EXE_WRITES=FORBIDDEN. This is a periodic receipt, not the exact
final Present count. No C0000005 was logged. Three first-chance C++ exceptions
were recorded; this does not establish unhandled failure or long-term stability.
User confirmed seeing VR; detailed stereo/lighting/controller A/B acceptance
was not completed. Read-only native cap observation: mode0,float60, two reads
identical. Effective cap remains UNKNOWN; runtime periods vary in existing
receipts, so neither an active60fps cap nor forced45Hz is established.

NO_GO for the requested complete CPU A/B: xperf's independent offline trace
statistics report3828807 lost ETW events (lost buffers0). PresentMon session
reported zero lost events and graceful session-stop exit0, but its process
ExitCode was UNKNOWN. A collector bug then called Regex.Matches on a null empty
xperf report, leaving the on-disk manifest pending. The raw trace/manifest have
been preserved; the separate offline verdict records these failures rather
than relabelling the capture valid. No B start or automatic retry occurred.

Existing xperf extraction was reused with -tle and the exact age182 frozen PDB,
local symbols only.23 raw-QPC/relative-time pairs give the same origin tick
494473745708. Retained combat stacks again include RecPush (1905 nominal sampled
ms) and VirtualQuery (1724), which OVERLAP. Because events are missing these are
incomplete sampled weights, not complete exclusive time or per-frame RUN totals.
No full RUN/BLOCKED/READY comparison, wake-chain verdict or causal batching
saving is claimed. The already complete older outdoor ETL remains the source
for the established1.830ms VirtualQuery /1.992ms RecPush overlapping exposure.
The completed WOW64 producer investigation was not repeated.

Useful private evidence: perf/constant-batch-ab-20261026T211559Z/A/
20261026T211925Z-b3b028f7, analysis-derived/offline-verdict.json and exact symbol
packet under dist/protected-constant-counts-20261026T210807Z/diagnostic.
ETL SHA256ef54212a8864a578df1f42ec0d5f5c3cdf77255dce64dd8162772b75f61bdfaa;
CSVf160b6b8c9bba9f3da341176ee0b960c65ce392e8338af2f5b9018c9c085e44a;
log243cecdc621f744b8d8e788b6e30712eb9bb3ffdd75abfb2db1eae039ab3bbfd.
Private logs, trace, game code and binaries are not committed.

The existing collector now preserves null/empty stats as UNKNOWN, reads xperf
loss counters with -tle solely for reporting (nonzero loss still rejects the
capture), retains explicit warning loss without double-counting, and retains
its owned PresentMon process handle before exit.23 real helper checks and four
PresentMon analyzer tests PASS. Owned offline process checks preserved actual
exit codes0 and37. The recorder regression has not been live-verified. Recording
buffer/disk-loss cause remains unresolved; no blind buffer or graphics change
was made. Renderer code/DLL stayed identical during and after A.

Rollback: original three Steam DLL/INI files and two local INIs restored, all
five SHA256 matches; original EXE SHA D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1
unchanged. Steam was not launched again. Batch stays OFF (benefit UNKNOWN), and
the separate implemented/offline-verified float-window candidate stays OFF.
A new complete comparison requires separately authorized Steam starts and a
loss-free CPU recording; this result does not establish a performance fix.
The current A mean is7.42872ms above the11.11111ms target under capture load.

## Architectural decision after cancellation of A/B preparation

BLOCKED for a justified high-impact renderer correction, NOT a game fix. The
previous A/B gate is cancelled by the user's later instruction. No package,
profiler, capture, Steam session or deployment. Unverified experiments stay OFF.

Native scene/material traversal already runs ONCE. RenderTrueStereo records live
eye0, invokes ReplayStream for eye1, then restores native state; replay does not
re-enter native material/object traversal. A second traversal cannot be removed.
The COMPLETE zero-loss outdoor B evidence (source804d0e7, PID43664/TID28400,
cpu-candidate-log-ab-20261026T183121Z/B/20261026T184245Z-9997b65a) remains the
source here, not the later lossy A or a new measurement of the current HEAD.
Existing cpu-phase-samples.json: partial=false, exit=0. Over1141 combat Present
rows, inclusive sampled weights: RecPush2273/1141=1.992112ms;
VirtualQuery2088/1141=1.829974ms INSIDE RecPush; ReplayStream456/1141=.399649ms;
RecEmit261/1141=.228747ms; CreateStateBlock95/1141=.083260ms;
CaptureBackbufferToShared545/1141=.477651ms. Do not add overlapping subtrees.
These are sampled attribution, not hard ceilings for UNKNOWN cross-architecture
descendants. RUN15.73932ms exceeds11.11111 by4.62821ms. Removing all identified
recorder/emit/replay exposure (~2.62ms) would still leave ~13.12ms. That does not
prove90Hz impossible or assign all remaining system-service cost to the engine.

Largest recorder shortcut considered: GetVertex/PixelShaderConstantF into owned
storage after Set, avoiding repeated external-source validation. Rejected with
REAL native D3D9 counterexamples, reproducible through the EXISTING harness:
`tools/d3d9-stereo-harness/build.ps1 -ConstantOwnership`. It uses only its own
hidden8x8 system-D3D9Ex device with observed flags0x50, no Draw/Present/Steam/XR.
For VS and PS, another thread legally SetConstantF OR Apply(D3DSBT_ALL) between
Set and Get: all calls return D3D_OK, Get returns the other writer, not the first
input. All four cases PASS; immediately owned raw snapshots remain identical.
This proves a legal race, not an observed game interleaving. RecLockGuard does
not hold the native device across Set+Get. Protected StateBlockHook is OFF;
unobserved native/private writes preclude exclusive state ownership. A local
setter lock or startup equality probe cannot repair it. Modified live-eye
constants also cannot recover original right-eye inputs from the driver cache.
No unsafe getter-capture implementation was added to src/.

Whole shadow-pass reuse is also unsupported: IsShadowAtlas is only a shape
heuristic; final atlas contents do not preserve earlier versions consumed by
right-eye draws. The historical run_35124 (different scene/profile, NOT outdoor
CPU timing) contains only101/4471 float uploads in its sole matching shape run.
No substantial measured saving or safe temporal-resource model justifies that
change. StateBlock creation exposure ~.083ms is below the requested threshold.
Removing MULTITHREADED conflicts with native workers; moving replay to a worker
would still share mutable native RT/state and does not remove that work.

Next architectural boundary required: owned immutable draw/material packets
before scalar D3D9 submission, or a backend owning both eyes' commands/resources
and versioned deferred passes. Native packet lifetime/private-call coverage is
not established for such interception. D3D9 SM3 has no automatic dual-view RT
routing: single-pass stereo requires shader/RT/deferred-pass translation.
No hash-protected EXE mutation, deferred raw pointers, skipped eyes or guessed
COM wrappers are acceptable shortcuts. Exact present blocker is the lack of an
atomic original-input snapshot at the current public Set/Get boundary.

Offline: four native D3D9 counterexamples; both x86 renderer builds; protected
Present six modes, image guard, head117021, input20032, gestures227, actual
recorder239 and actual PS payload576 per build mode PASS. Renderer and INI
sources remain identical to ebecf4f. Saving NOT VERIFIED; no integrated Steam
candidate from this decision. This is a demonstrated safety blocker, not an
optimization or a claim that the outdoor performance issue is fixed.
