# Protected outdoor critical path — 2026-10-26

Status: measured persistent CPU bottleneck remains UNRESOLVED. A separate,
reversible regular-log I/O candidate is being prepared; no 90 Hz or headset
performance acceptance is claimed. No resolution/FOV/stereo/input change.

## Provenance and capture limits

Runtime DLL code: `2e70cc4ee4e2283c05c5cb1b4f78cbf81f0df4d3`, SHA-256
`bdc7fd06b02c9a05242ab40a4e90999bb7b760482d940c6225d4e89a3b7e2b5f`.
EXE SHA-256 `d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
PDB RSDS/MSF identity matches: `dd21a35c-1536-40c1-b9b3-9ffa3f10cc44`, age4.
Private trace: `perf/protected-outdoor-retry-20261026T162352Z/20261026T162734Z-f0a7a06e`.
PID42740; render42504; NVIDIA worker13164. Source remained1920x1071;
requested2048x2091 is not evidence of actual rendering size. XR2676x2728.

The first attempt had no visible XR scene and is REJECTED as VR performance
evidence. User explicitly authorized a retry and confirmed seeing the indoor
level in PSVR2. UAC finished before the replacement Steam launch. Four external
phase cues instructed indoor10s, exit15s, outdoor10s and combat25s. These are
user-performed phases, not an independent engine-level scene classifier.

The original manifest is preserved as incomplete: an xperf informational stderr
loss warning threw under PowerShell5 before the final receipt. The derived
manifest conservatively caps combat at the scheduled25s deadline; it does not
turn the capture into PASS. ETL lost1,026,324events,0lostbuffers. Retained WPR
system-loss notices lie at52.082235..54.702816s, affecting CSwitch, ReadyThread,
samples and stacks. Two GPU-loss notices occur at49.539681 and51.883193s.
No WPR loss notices were observed in the three precombat phases; absence is
not certification of losslessness. Unknown scheduler intervals remain UNKNOWN.
Loss notifications cannot partition the merged-header total or prove RAM/AV cause.

304 raw-QPC/relative-time pairs give origin319370277797 at10MHz:
`ETL microseconds=(QPC-origin)/10`. PresentMon intervals are app timing, not
physical headset display FPS. Combat's inferred endpoint and ETW loss apply to
all reported aggregates. ETL/CSV/private logs/binaries remain outside Git.

## Actual frame cost

- indoor: 708 fully contained app intervals; mean 14.178 ms, p50 13.742, p95 17.845, p99 20.715, worst 27.351; mean GPU Busy 2.902 ms.
- transition: 1052 fully contained app intervals; mean 14.266 ms, p50 13.858, p95 17.237, p99 24.465, worst 63.312; mean GPU Busy 2.890 ms.
- outdoor: 490 fully contained app intervals; mean 20.429 ms, p50 20.074, p95 25.480, p99 31.281, worst 82.628; mean GPU Busy 3.818 ms.
- combat: 1220 fully contained app intervals; mean 20.467 ms, p50 20.373, p95 24.498, p99 26.924, worst 72.712; mean GPU Busy 3.732 ms.

Render scheduled-running indoor8,881.549ms/708frames=12.545ms/frame;
outdoor8,783.036/490=17.925ms/frame. Outdoor blocked973.145ms=1.986ms/frame,
ready137.818ms=0.281ms/frame, unknown120.503ms=0.246ms/frame. Scheduled-running
includes retained interrupt residence; xperf excluding DPC/ISR reports17.899ms.
This contradicts a hypothesis that the ordinary20ms frame is mostly blocked.
The ordinary outdoor problem is predominantly execution before GPU submission.

Render retained sampled weights approximate12.733ms/indoor frame versus18.084ms
outdoor. RecPush-inclusive1.496→2.486ms; WOW64 query-special subtree1.925→3.251ms
OVERLAPS RecPush and cannot be added. WOW64 memset is syscall housekeeping, not
proof of arena zero-fill cost. Replay-inclusive0.338→0.506ms; shared capture
0.668→0.659ms; reader0.380→0.320ms; XR copy0.148→0.145ms. No transfer-cost growth
explains the slowdown. NVIDIA worker executes in PARALLEL, not serially with render.
A single read-only/noanalysis Ghidra query of the pinned EXE identified
AC3809 inside FUN_00AC36F0 (graphics resource/reference-list collection),
BF2849 inside FUN_00BF27A0 (texture/sampler binding and shadowed stage state),
and9F210E inside FUN_009F2030 (intrusive-child-list dispatch). Class names remain
UNKNOWN. These exact PCs carry47/17/15 outdoor samples respectively; they do
not individually explain the full growth. Private driver symbols remain UNKNOWN.

## Exact costly waits in the ten worst frames

The accompanying CSV joins the same QPC intervals, scheduler partitions, actual
wait-return/readying stacks, and retained GPU packet unions. Wait categories are
DISJOINT render-blocked segments, not total API duration and not additive to
parallel driver/GPU execution. A switch-in return stack identifies the wait;
where missing or truncated, ownership stays UNKNOWN.

- Frame82.628ms: CreatePixelShader blocked45.710ms, native CreateQuery9.668ms.
  `NtWaitForSingleObject → nvd3dum → CD3DDDIDX10::CreatePixelShader →
  CD3DBase::CreatePixelShader → Hook_CreatePixelShader`. Render readied by13164;
  driver itself waits on worker fanout, e.g.43256. CreateQuery caller009FBDE5.
- Frame78.157ms: PS35.031ms, VS6.767ms, CreateQuery13.047ms.
- Frame72.712ms: regular `File::WriteV → fflush → WriteFile → NtWriteFile →
  FLTMGR → NtfsCopyWriteA → NtfsAcquirePagingShared → ExAcquireResourceSharedLite`
  blocked47.833ms. System43608 releases the NTFS resource through cleanup /
  prefetch-close. Driver waits48.003ms for render. No Bitdefender module is in
  this retained path; do not call it an AV wait. The stack stops at WriteV,
  so the exact log-line caller is UNKNOWN. No exception is logged nearby.
- Across these ten frames, distinct retained render waits total PS154.774ms,
  VS31.397ms, CreateQuery53.592ms, log flush47.833ms, Steam15.227ms and producer
 13.375ms. The35.183ms frame includes13.359ms in steamclient; lock purpose is
  UNKNOWN. The47.674ms frame includes7.538ms producer GetData; required resource
  synchronization is retained.

The shader/query waits are now proven on this protected outdoor session, not
borrowed from historical indoor/EXE-patching captures. Native PS getter00493180,
VS00846E70 and binder00B4D2B0 are the existing static ownership findings. Their
loading/retention/exclusive-access gate remains unproven; no unsafe prewarm,
shader COM reuse, native patch or skipped wait is introduced.

## GPU, XR and frame budget

Outdoor retained submit-to-execution mean0.053ms,p950.214ms,max2.098ms. The
82.628ms frame contains5.167ms retained game GPU union and2.879ms compositor
union, not80ms of GPU execution; queue maximum0.511ms. Missing DMA starts mean
uncovered intervals are NOT proven GPU idle.3,149 game DMA stops are unpaired.
D3D9/D3D11 ownership of both native contexts cannot be named from GPU ETW alone.

Matched five-second CPU-sync windows show boundary3.214→3.815ms while app-frame
mean rises6.251ms. xrWaitFrame~0.023ms and original Present~0.664ms do not explain
that growth. Timed D3D9 live/replay elapsed spans include submission gaps, not
isolated GPU Busy. Nested scopes are not summed. SteamVR whole-PID session:
16,702presents/6,564reprojected/38dropped; per-phase reprojection UNKNOWN.
22.222ms predicted XR period does not prove a45Hz display or90 unique game frames.

Mean outdoor20.429ms exceeds native90Hz's11.111ms budget by9.318ms; render
scheduled-running alone exceeds it by6.814ms. Removing the observed logging
spike cannot be represented as removing this entire sustained deficit.

## Narrow correction and remaining gates

The exact WriteV flush stall justifies moving NORMAL log-file I/O to a bounded
worker whose disk operations cannot hold the producer queue lock. Preserve
persistent handles, ordering/drop accounting, emergency fatal-exception output,
loader-lock-safe shutdown and the default synchronous baseline for an isolated
A/B. This addresses a measured47.833ms spike, not the entire outdoor CPU cost.

Do not cache permissions of arbitrary engine constant buffers: they are not
owned by the mod, and SEH-only copy can consume PAGE_GUARD. Separate caps-cache
and small-copy candidates must remain isolated from the log hypothesis.

Both starts exited Steam0; no C0000005 recorded. This bounded survival is not
long-term stability acceptance. Original DLL and all four INIs restored5/5 with
matching hashes; EXE unchanged. No further Steam launch is authorized. A new A/B
requires user authorization after the concrete correction passes offline tests.
The postprocessing stderr fix preserves the phase/PM counters before xperf and
uses native exit codes; it cannot retroactively repair the old raw manifest.
