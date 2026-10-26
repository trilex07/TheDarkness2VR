# AsyncLog outdoor A/B and the next CPU correction

Decision: KEEP AsyncLog as an opt-in, default-OFF isolation of ordinary file
I/O. Do not promote it as a measured FPS/stall-elimination fix. Neither arm
reproduced the previously observed 47.833 ms flush stall. The sustained outdoor
CPU problem and 90 Hz target remain UNRESOLVED.

## Comparable configuration, non-equivalent workload

Existing authorized session cpu-candidate-log-ab-20261026T183121Z only; no new
Steam launch. A/B used one frozen diagnostic DLL, SHA-256
4F8A9A455F4413B9BB5ADEE6022D0CA89D90DB3228895D7F38D6A518A923BC2E,
source `804d0e7`, PDB GUID 26b151b8-b2a9-4a6f-b1fc-8cc227629edc age 169.
The sole effective setting difference was Render/AsyncLog 0/1. Head, controllers,
gestures, stereo, resolution and image-preserving Present stayed unchanged.
SamplerCapsCache, SmallRecordCopy and LastConsumerReaderFence were OFF.

PID/renderTID: A 36200/42616, B 43664/28400. QPC frequency 10 MHz; independently
verified ETL origins 395687297425/400481588994. Fully contained phase-frame
counts 611/1192 and 553/1140 exclude one final crossing frame per phase; raw
manifest counts 612/1193 and 554/1141 must not silently replace them. Precise
scheduler spans are clipped to each retained PresentMon frame. RUN includes
scheduled kernel/interrupt exposure; it is not purely game instructions.
Sampled leaf estimates use nominal1ms weights over the marked phase and raw
phase Present counts, not exact per-frame execution. Partial WPA exports and
the first incorrectly associated wait-stack pass are excluded. The successful
existing xperf parser associates saved CSwitch stacks with the switch-IN TID's
previous wait, and preserves external-process readying stacks. Render saved
stack coverage 56809/56809 and 64210/64210 is nonempty; native caller depth can
still be UNKNOWN.

Both traces report zero lost ETW/PresentMon events. Raw manifests nevertheless
remain invalid_or_incomplete solely because PresentMon PROCESS exit is null
(`PresentMon exit code: `). Its stop-request exit is0; CSV covers both completed
phases. Data are usable, not a fully validated capture PASS. Do not rewrite raw
manifests, convert null to0, or discard the usable trace. User reported normal
closure in both arms. Original DLL plus four INIs restored 5/5; fresh hash check
and original EXE SHA D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1
PASS after offline work. No further launch occurred.

## Actual results

All four stationary/combat distributions and threshold counts are in
`async-log-ab-frame-metrics-20261026.csv`. Combat mean A 16.784 / B 17.551 ms,
p50 15.654/17.043, p95 23.195/21.828, p99 26.860/25.399, worst 116.662/33.757.
Counts above 11.11/22.22/33.33 ms: A 1161/106/3 of 1192; B 1140/50/1 of 1140.
Stationary means16.440/18.136ms; B also has a78.175ms stationary spike.

Combat scheduled RUN/BLOCK/READY: A 13.169/3.373/.242 ms per complete frame;
B 15.739/1.500/.311. Missing 90 Hz wall budget5.673/6.439ms. The means are
~59.58/~56.98 application Presents/s, an upper bound on distinct world frames,
not90 unique rendered frames.
There is no demonstrated universal 45 Hz lock. Phase-contained XR windows in A
report world/layer pairs48.8/48.8,33.8/67.0,46.4/71.4; B55.8..60.6 has
world=layers. Sparse windows do not establish the entire phase's unique world
rate, and submitted layer frequency cannot replace it.

Observed blocked WriteV/fflush/trueWriteFile episodes are0 in ALL four phases.
A File::WriteV sampled CPU exposure1/4 nominal samples (baseline/combat),
with4 combat fflush samples NESTED, ~.00336ms/combat frame, not an extra cost.
B render has0 File::WriteV/fflush samples,2/2 Async::WriteV samples. Other
subsystems still sample WriteFile (combat A34/B24); do not claim every file
operation vanished. Writer B16844 ran2.881/2.769ms TOTAL per phase; its
10044.298/20024.485ms blocked time is idle/event WaitForSingleObject, not a
filesystem stall. No file-write wait was observed on that worker.

A's dominant blocked difference is260 native SleepEx episodes totaling2437.457ms
in combat, absent in B. The wrapper return0099813D alone does not reveal its
caller or requested delay. NVIDIA shader waiting episodes7/19 in A vs62/8 in
B stationary/combat; combat durations41.558/4.520ms. Episodes are not creation
counts. This different shader placement and native sleep exposure prevent a
causal AsyncLog interpretation of improved percentiles or worse mean.

`async-log-ab-blocked-paths-20261026.csv` contains disjoint categories/counts;
`async-log-ab-worst-ten-20261026.csv` contains both arms' worst ten combat frames.
A116.662ms =104.862RUN+11.572BLOCK+.228READY, not a100ms file wait.
A40.795/36.687ms contain19.365/17.777ms shader waits. B33.757ms contains
14.221ms Steam IPC waiting, readied by steam PID4828/TID8816. Whole-process
SteamVR cumulative records exist (A18392 presents/30 dropped/6276 reprojected;
B17236/29/4915), but include startup and time outside measured phases. They
cannot be used as matched combat reprojection percentages.

GPU Busy combat2.903/3.483ms (~20% more in B); stationary3.184/3.324ms.
Do not equate PresentMon GPUWait13.231/13.749ms with CPU blocked time or
complete GPU queue idle. Existing CpuSyncTiming phase-contained windows give
boundary wall3.270/3.321ms, originalPresent.640/.664, xrWait.082/.023 and
xrEnd.630/.639; these are nested API/wall observations, not additive CPU waits.
Recorded sources1920x1071, cache/XR2676x2728 in both arms; requested2048x2091
was not applied. Do not change resolution to explain the A/B difference.

## Queue and critical diagnostics

B's last visible counter snapshot: enqueued2818, written2817, dropped42,
truncated0, errors0. Drops are ordinary records, NOT ETW loss. Full queue vs
try-lock contention and exact phase of loss are UNKNOWN; snapshots are not an
exit-drain proof. Ordinary tail records may be lost under loader-safe shutdown.
The independent preopened native-emergency handle was active; its file is
empty because no native fault was observed. Live fatal-write reliability was
NOT exercised. Existing blocked-writer/native-exception tests verify the
separate emergency path offline. Neither the queue nor producer becomes
unbounded/blocking. Keep opt-in, preserve explicit loss counters and emergency
logs, and retain AsyncLog0 rollback. Do not request another logger-only cycle.

## Sustained work: chosen correction and limits

Disjoint per-module sampled leaf costs are in
`async-log-ab-exclusive-modules-20261026.csv`; phase raw-row denominator is
explicit. Native game combat ~2.914/3.689ms, identified proxy C++~.688/.787,
NVIDIA user modules~.749/.845 and system/ambiguous D3D9~.761/.940. Other module
leaves, including kernel, are separate. Inclusive RecPush1.626/1.992ms overlaps
VirtualQuery1.493/1.830ms; do NOT add these. AVX ntdll memset1.293/1.518ms has
UNKNOWN recovered producer in all1543/1732 retained combat samples; no evidence
permits attributing it to vector zero-fill or removing it. Larger native/kernel
costs are not safely corrected by guessed instruction skips or page caches.

The next isolated implementation is SmallRecordCopy in
`src/small_record_copy.h` and actual `src/stereo_replay.inl::RecPush` (804d0e7;
actual-body tests aecf54a). It replaces fresh VQ+copy only for1..192B with
exact-count self-RPM, rejects uncertain guard semantics before installing
hooks, and falls back to unchanged fresh-VQ prefix/zero-tail handling on any
short/error result. No permission cache, image writes, resource/fence or stereo
change. Current helper remains OFF. Public separate on/off overlays in
`profiles/protected-small-record-copy` pin all other experiments OFF and preserve
head/input/gestures. Current source guard/default checks and actual-body tests
PASS. A real game A/B is the next gate, requiring new authorization; NOT RUN.

Five existing historical traces reconstruct23673 pushes with zero arena gaps:
23142 (97.76%) fit1..192B,67.48% of bytes. Their3076x3140/wide profile differs
from current outdoor; current payload-time coverage is UNKNOWN. Existing
microbenchmark small valid-copy gain~27% is NOT a game FPS forecast. Even
removing the ENTIRE1.99ms RecPush subtree would leave more than 4 ms of B's
90 Hz wall-budget deficit. Do not promise90Hz from this candidate.

Larger safe opportunities were checked before selecting this partial target:
steady source VQ is entirely dynamic RecPush data; Present/XInput/source-size
identity VQ already short-circuits after installation. Native game/private
memset ownership remains unproven. Shader helper series8359d014+8d4779dd is
NOT integrated: old Hook_CreatePixelShader CPU exposure ~.020ms/outdoor frame
and it does not remove the large native NVIDIA creation waits. Its standalone
8359d014 also needs the guard-range safety correction. Native owner-retained
prewarming remains blocked by lifetime/device/loading-phase proof; no hash-only
reuse or async D3D9 calls.

Fresh x86 production+diagnostic builds PASS, without Install. Regressions:
actual recorder239 checks; Async370+activation11+emergency-failure24 and10000
persistent records; all six image-Present modes; XR lifecycle117; gestures227;
XInput20032; native image guard; profile composition7 and PM analysis4 PASS.
Installed files remain the restored baseline. No new runtime/HMD performance
claim or FIXED status is made.

## Bounded native frame-pacer finding (not a deployed fix)

Verified-code SleepEx wrapper00998130 has bytes
8B4424046A0150FF1558B3EE00C3;0099813D is its return. It forwards caller-supplied
milliseconds with alertable=TRUE. There are multiple unrelated callers, so the
measured sleep cannot be removed or labelled a frame limiter from that return.
Static client00938B62 is a deadline loop: compute remaining ms, SleepEx, recheck
the QPC-based clock. Registration00EAACC0 names Graphics.MaxFrameRate and
initializes 60; one loop branch uses its reciprocal. Current effective value and
measured caller remain UNKNOWN. Default 60 is not proof of an active 60 Hz cap.
Even a confirmed 60 Hz cap would not explain away B's 15.74 ms scheduled CPU work;
raising it alone cannot meet the 11.11 ms budget.

The actual AppData/Roaming/DarknessII/EE.cfg is encoded, not a validated INI
surface. Neither a verified decoder/round-trip nor public override/setter was
established in this bounded pass. No guessed config edit or write to image data
010EB094 is permitted. This is a concrete remaining blocker, not a second
speculative patch. In a future separately authorized session, the missing
read-only values are FrameCapMode@010EB068, MaxFrameRate@010EB094 and one
actual SleepEx caller/argument; caller return00938B67 selects the proven deadline
loop. No additional general profiling session is required for that distinction.

Fresh installed/private short-range comparison also PASS: Graphics string
00F38AD8, MaxFrameRate string00F8BF18, FrameCapMode string00F2B540, float
00FC8034 bytes00007042 (=60.0), and initializer instruction ranges00EAACA5,
00EAACD6,00EAACED. Thus names/defaults do not rely solely on matching.text from
a different private PE hash. Runtime property values remain UNKNOWN.
