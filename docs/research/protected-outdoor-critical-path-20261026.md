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


## Concrete logging candidate (isolated from other optimizations)

MAIN integrates the bounded-worker correction from `codex/async-log` on
`codex/outdoor-frame-bottleneck`. Ordinary producers format/enqueue with a
nonwaiting try-lock; the writer releases the queue lock before file I/O.
`Render/AsyncLog=0` remains the default and is the same-DLL A arm; `=1` is B.
Both keep the frozen protected stereo/head/input/gesture profile and identical
resolution. No caps cache, small-copy experiment, native hook or XR recovery
change is included. The full native exception witness has independent emergency
output. Admission failure is not an accepted performance run.

Status: IMPLEMENTED and OFFLINE VERIFIED on MAIN integration branch.
IN-GAME VERIFIED: NOT RUN. HMD ACCEPTED: NOT RUN. No FPS gain or 90 Hz claim.
The existing collector now preserves both normal and emergency logs with
SHA-256 snapshot receipts. These snapshots are not atomic frame witnesses.

This targets the observed 47.833 ms flush stall, not all sustained CPU work.
The next largest spike path remains native/driver CreatePixelShader and
CreateVertexShader. Existing shader loading-phase ownership/lifetime findings
still block a safe runtime prewarm; no asynchronous D3D calls, hash-only reuse
or skipped synchronization are admitted. Separate caps and small-record-copy
candidates remain on their own branches and are not mixed into this A/B.


### Final integrated offline acceptance

Integrated `dfa7c54` -> `2726bb5` and `ad68a6d` -> `cf86694`; no other SIDE
optimization was cherry-picked. Final actual-source regression: 370 queue/
producer/VEH checks, 11 failed-activation checks, 24 real emergency-CreateFile
failure checks and unchanged 10,000-record persistent logger regression PASS.
The unavailable emergency-sink failure preserves full native fault records via
an exclusive native-VEH-only fallback; ordinary producers never use that path.
Protected image guard PASS; head 117,021, XInput 20,032 and gesture 227 checks
PASS. Existing collector helper 16 and PresentMon analyzer 4 tests PASS.
Production and diagnostic builds PASS. These are offline results only.

Frozen package (private/untracked):
`dist/protected-outdoor-async-log-20261026T173448Z`.
Artifact source `cf86694`; diagnostic DLL SHA-256:
`717d5ab98bf9c710f72aba326583a8fd8b8da17f13411028098838fc0335358b`.
Diagnostic RSDS matches the frozen PDB: GUID
`26b151b8-b2a9-4a6f-b1fc-8cc227629edc`, age 163. Production pair age 162
also matches. Windows GetPrivateProfileSection/IntA verified A/B base INIs
against the frozen baseline: only Render/AsyncLog differs, diagnostic INI is
byte-identical. No UTF-8 BOM is introduced into the packaged ASCII INIs.
Both arms use the same diagnostic DLL; all other keys remain unchanged.

Current Steam installation was rechecked after offline work: original DLL plus
four INIs match all five rollback hashes; original EXE SHA-256 still matches.
No game process was running at that check. No deployment or new launch occurred.
Fresh user authorization and free-session check are required for a comparable
same-scene A/B. Measure unique game-frame percentiles, render running/blocked/
ready spans, disappearance of the ordinary flush wait, queue losses/errors,
NVIDIA creation waits and stability; do not infer FPS gain from worker tests.
The retained trace has the documented event loss, so an exact lossless total
CPU/GPU attribution remains unavailable. The outdoor 9.318 ms budget deficit
is unresolved until actual measured improvements close it.


### Authorized A/B attempt: stopped before performance recording

User explicitly authorized two 10-second-stationary +20-second-combat starts.
Fresh original EXE SHA/PE/site/range audit PASS. Only SteamVR and fpsVR were
running. All five installed DLL/INI files backed up with matching SHA-256.
Arm A used the frozen diagnostic DLL above, AsyncLog=0; ordinary worker was
not started. The HMD user reported a flat game screen, so no WPR/PresentMon
recording was triggered. B was NOT RUN; no automatic retry is authorized.

A's log installs all three observer entries at exactly the same targets as the
successful prior protected trace: Create6471CE30, QI6470D820, Present64683E30.
It remains EXTERNAL_PENDING through28,200 scenes and has no qualified acquisition
or native Present receipt, InitXR or VrFrame. QI/Present installation occurs only
inside Observe() after typed acquisition, so this is not proof that all raw
Create/Present thunks were absent. Current logging filters out unqualified
Create and Present calls. Missing evidence is their actual caller/provenance
and forwarding counts before admission. Do not claim zero raw Present calls.

Observer/runtime source is byte-identical between frozen2e70cc4 and98442ef.
AsyncLog=0 returns before pin/lock/file/thread work. The inherited disabled
reader-fence code runs only after the XR path; it is unchanged from integration
base010a84e and legacy-order regression PASS. Inheritance versus the older2e70
binary is explicitly retained, not erased: the new A/B arms use the SAME DLL
and the existing reader experiment remains OFF. A rebuild/layout/startup-timing
interaction is still UNKNOWN; logger and fence are not proven causes.

Admission guards remain unchanged. A verified native Present still requires a
qualified typed acquisition. Existing fixture always qualifies Create, leaving
this rejection path untested. A separate bounded observer-receipt candidate will
cover unqualified Create followed by native Present, proving transparent
forwarding while keeping XR closed. No alternate interception, EXE write,
EndScene fallback, new profiler or speculative admission relaxation is allowed.

The user closed A normally. Original DLL and all four INIs restored5/5;
original EXE hash unchanged. Private runtime receipt:
`runtime/async-log-ab-20261026T174216Z` under LocalAppData/Darkness2VR.
The elevated helper never received a start flag and has only its bounded idle
timeout; there was no recording session. A/B frame numbers and async-log FPS
improvement remain NOT MEASURED. Logging candidate is OFFLINE VERIFIED only;
VR admission is BLOCKED, not HMD accepted or FIXED.

## Follow-up: focus interruption and bounded recovery candidate

The separately authorized 3174a2f A arm passed real additional-swapchain
acquisition and native Present: caller00964CC1 qualified Create; caller00A94057
qualified Present, matched entry/forward/return, HRESULT0 and recursion0. XR
initialized. After the elevated console/UAC prompt, the user reported losing VR;
XR changed4->3, layers became0, and Present eventually returned positive
0x08760877 (once0x08760878). Exactly2937 D3D_OK returns were recorded before
readiness remained closed. Those positive statuses are not successful stereo
acceptance. The process later exited; its exit cause is UNKNOWN. Only handled
C++ exceptions were retained, with no recorded C0000005. This is not a stability
PASS. No collector start flag was written, no ETW/PresentMon capture started,
and B was NOT RUN. The helper subsequently timed out normally at18:17:13Z.

Original DLL and four INIs restored5/5, original EXE SHA unchanged; rechecked
after offline builds. The last session is private under
runtime/admission-async-ab-20261026T180552Z. Do not retry automatically.
A future authorized collector must finish elevation BEFORE launching Steam/VR,
then require actual visible loaded gameplay before starting the capture.

Integrated existing event recovery f17726b as0e8cc13. A qualified outer native
Present on the latched render thread can pump existing XR events while frame
admission is closed. It does not obtain a device, replay, render or submit an XR
frame on that closed path. A later D3D_OK still rewarms only the following frame.
The first Present returning occluded can already have run its former ready path
before the HRESULT is known. This correction cannot restore desktop visibility
or run if native Present stops. Runtime recovery remains NOT VERIFIED.

## Disjoint CPU budget and next bounded optimization

`protected-outdoor-exclusive-cpu-20261026.csv` assigns every retained sampled PC
to exactly one leaf category. It complements, not replaces, the precise
scheduler timeline and the documented ETW loss/partial stacks. Estimated sampled
running12.733ms indoor and18.084ms outdoor agrees with scheduled running12.545
and17.925ms within0.19ms. It is not an API invocation count or a measurement of
unprofiled execution. Native game3.873ms, query-named leaves2.437ms, unresolved
ntdll memset2.186ms, syscall dispatch1.280ms, identified proxy C++1.231ms and
NVIDIA render-thread leaves1.041ms are disjoint outdoor categories. Named ETW
stack/unwind leaves themselves cost1.002ms outdoors; some measured overhead is
profiler work. GPU Busy remains3.818ms while app intervals average20.429ms.

Do not add inclusive RecPush2.486ms to WOW64/query3.251ms. Indoor full stacks
prove949 overlapping samples: the union is2.081ms, whereas adding inclusive
counts would give3.421ms. Outdoor exact intersection is UNKNOWN. All retained
indoor memset samples lack recovered mod/query ancestry; attributing this work
to vector initialization is unsupported. Direct vector resize leaves0.029ms,
COM/reference leaves0.178ms and recorder lock leaves0.243ms are already contained
in the categories above. They do not justify a speculative multi-ms rewrite.
Native AC36F0/BF27A0/9F2030 groups total0.288ms; other native PCs remain unresolved.

Integrate sampler-caps49cd725 asb467940, then its explicit A/B gate612bca0 as
305b7a3. Render/SamplerCapsCache remains0 by default: the exact legacy resolver
and current requested anisotropy/clamping remain unchanged. Opt-in caches only
positive capabilities per thread/device generation. Entry to HookDevice, Reset
and ResetEx invalidates even on failed reset/early return. No COM reference or
borrowed source pointer is retained; errors and zero caps are never cached.
The measured GetDeviceCaps subtree0.500ms is INCLUSIVE (FillInCaps0.494ms nested),
not a promised saving. This removes a concrete repeated query, not the entire
9.318ms outdoor frame-budget deficit. No native FOV, resolution, shader, hand,
EXE image or resource-synchronization changes; no INI/profile enabled by default.

Integrated checks: sampler3705; real HAL material/stateblock424; XR recovery117;
all six Present observer modes incl ABI/Reset/lifetime/overlay/negative HRESULT;
async producer/VEH370, failed activation11, real emergency failure24 and10000
persistent records PASS. Protected image/integration Python3+6 PASS. An initial
unittest module invocation collided with Python's stdlib test package; direct
repository test entry points pass. Both x86 production/diagnostic builds PASS.

Frozen source305b7a3: production DLL8E125A9DF9A1D0D3C0B3FB0C84247884D76D8E74A4A4303C8A04C247C936EB8E;
diagnostic9FCC1D2A53910521CD755B4750BF9E85F0BCB368FDB421737885C59911F76F96.
Both PDB identities match RSDS, GUID26b151b8-b2a9-4a6f-b1fc-8cc227629edc,
ages166/167 respectively. Binary/PDB/map packages remain private/untracked.
AsyncLog and SamplerCapsCache remain independent OFF-default experiments; any
later A/B changes ONE flag with a common DLL and fixed image/profile. Steam and
HMD for these integrations: NOT RUN. No measured gameplay improvement or90Hz.
The next justified candidate is the separately reviewed small recorder copy,
not a native-loop rewrite: it addresses eligible fresh VirtualQuery calls only.
