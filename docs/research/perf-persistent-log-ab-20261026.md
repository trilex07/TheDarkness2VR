# Persistent-log A/B: wait elimination verified, combat and stability blocked

Branch: `codex/perf-critical-path`. Compared exact diagnostic code from
`4665f73` (A) and `7eb665a` (B). This is an actual Steam/PSVR2-assisted attempt,
not another simulated performance result. It is **not a completed combat A/B**.

## Outcome and scope

The user authorized A/B and confirmed Steam/PSVR2 availability. Fresh installed
EXE SHA/PE/instruction audits passed. DLL and both INIs were backed up and
SHA-checked. No other Steam game was observed running. Both attempts used the
same unchanged INIs, SubmitMode=4, StereoMode=1, capture OFF, NIS OFF and actual
2048x2091 shared source. No hand, tentacle, FOV or resolution correction was mixed in.

A exited during capture after C0000005 at 00E129EA. MAIN restored the baseline
and obtained separate explicit approval for one B attempt after this failure.
B also exited after C0000005 at 00E129EA, before its combat marker. No third
launch or new capture was performed. Both original capture manifests remain
invalid; neither was rewritten to PASS. Both ETLs and PresentMon sessions report
zero lost events/buffers.

Only the first **8 seconds after each stationary marker** are compared below.
Complete contained frames: A=567, B=641. B has **zero combat PresentMon rows**.
A has 666 combat rows before exit, so its combat numbers have no matched B.
The available evidence proves removal of a specific wait; it cannot establish
sustained combat FPS, 90 FPS, or HMD acceptance.

## Actual before/after measurements

Milliseconds unless stated. A -> B, same 8-second stationary duration:

- FrameTime mean: **14.082 -> 12.461**; p50: **13.662 -> 12.098**.
- p95: **16.407 -> 13.737**; p99: **25.279 -> 21.467**.
- Worst frame: **72.738 -> 130.105**. Tail spikes were not eliminated.
- Frames >11.11ms: **566/567 -> 541/641**.
- Frames >22.22ms: **9/567 -> 6/641**; >33.33ms: **2/567 -> 2/641**.
- Render actual execution excluding IRQ/DPC: **6832.883 -> 6956.816-6956.819**.
- Render blocked: **868.120 -> 820.467**; ready: **147.231 -> 122.343**.
- IRQ/DPC while scheduled: **151.646 -> 100.303-100.306**.
- PresentMon GPUBusy mean: **3.803 -> 3.380**.

CPU is precise whole-window time, not PresentMon CPUBusy. B executes more frames
in the same eight seconds; total execution need not fall when per-frame work
falls. Do not add overlapping CPU, GPU and coarse stage measurements.

### The logging correction worked on the observed path

A render TID30920 has **80 Log/CloseHandle/filter waits totaling 168.229ms**.
Their wake source is bdservicehost PID2568; stacks pass through Log and the
file close/filter path. In B render TID36804, that path has **zero identified
blocked spans** and no bdservicehost wake-source time in the comparison window.

Remaining Log/fflush work is not assumed free. Sampled CPU has 17 inclusive Log
samples in A (7 flush/write, 7 close, 3 other), versus zero observed in B.
B's sampling period is 1000us. Zero samples do not prove zero CPU cost, and the
exact per-call flush execution time was not instrumented. Per-line fflush and
the existing lock remain in code. This is not asynchronous or buffered-loss logging.

B's original PDB was unavailable after the production build. MAIN rebuilt the
unchanged diagnostic source offline and verified the **entire .text section,
RVA and bytes identical** to the measured DLL (SHA256 of .text:
`c0f55794c87031ce5e0fa871d9d42db973fdc9ef4443317fdb1498db10a6a18c`).
Only that module's recorded base plus verified code map was used to name its
B frames. Missing private NVIDIA function names remain UNKNOWN.

### Important comparison limits

The observed p95 reduction is 16.3%, but cannot all be attributed to logging.
The scenes were manually loaded, not deterministic replays. Shader-creation
blocked segments differ (A=8, B=56), as do GPU workload and runtime pacing.
The nearby runtime predicted display periods are **22.222ms in A and 11.111ms
in B**; compositor settings were not changed by MAIN. That may be a runtime
response or a confound, not proof of a changed headset refresh setting.
The isolated, directly attributable result is the eliminated file-close wait.

## GPU, transfer, replay and XR

Game DMA execution union / 8 seconds: **2593.013ms A -> 2456.505ms B**.
Without game DMA: **5406.987 -> 5543.495ms**; this does not mean the whole GPU
was idle. Mean queue submit-to-start: **0.126 -> 0.079ms**, maxima 2.642/2.589ms.
Longest no-game-DMA gap: 24.235/56.245ms. No unmatched DMA stops in either export.
The long B gap agrees with CPU-side shader preparation, not queue saturation.

Existing 120-frame GPU elapsed-timestamp batches near the selected window:
A live=6.070-6.584ms, replay=2.634-2.725ms;
B live=4.651-7.105ms, replay=2.444-2.602ms. These include intervening idle time
and are not busy-time measurements or exact eight-second/per-frame markers.

Existing five-second wall-time batches near the window:

- D3D11 reader fence mean: A=0.396-0.411ms; B=0.343-0.347ms.
- D3D9 producer fence mean: A=0.578-0.582ms; B=0.564-0.660ms.
- xrWaitFrame mean: A=0.023ms; B=0.071-0.225ms (B max 10.772ms).
- xrWaitSwapchainImage mean: A=0.002-0.003ms; B=0.002ms.
- xrEndFrame mean: A=0.625-0.636ms; B=0.614-0.619ms.
- Actual forwarded Present mean: A=0.643-0.648ms; B=0.593-0.608ms.

No required fence, copy or XR synchronization was skipped. ETW classified
OpenXR-side blocked time is 136.548ms A / 180.836ms B across the whole eight
seconds, rather than one long deadline responsible for every slow frame.

SteamVR retained per-application **whole-session** statistics:
A=11770 presents / 29 dropped / 2467 reprojected;
B=8272 presents / 28 dropped / 1246 reprojected.
Its whole-session application CPU/GPU averages are 11.884/2.417ms A and
10.650/2.075ms B. Sessions have different durations and include startup/menu;
these are not matched combat results or an eight-second reprojection comparison.
No per-frame physical PSVR2 acceptance was obtained.

## Next established bottleneck and native cache evidence

In B's worst **130.105ms** frame (ETL start 2.8794144s), **97.865ms / 32 blocked
segments** belongs to Hook_CreatePixelShader/VertexShader -> D3D9 -> NVIDIA
semaphore waits. Shared transfer contributes 5.539ms, other NVIDIA waits 0.553ms,
OpenXR-side waits 0.185ms, unknown 0.287ms. Remaining time includes execution,
ready delay and interrupts; these are not added to driver-thread execution.

Across B's eight seconds: shader creation waits=141.873ms/56 segments;
other NVIDIA wait stacks=245.445ms (private function UNKNOWN). Thread execution
still dominates the overall frame budget. This correction does not solve 90 FPS.

Read-only Ghidra on the SHA-pinned EXE establishes existing **native lazy caches**:

- PS function 00493180, call at 00493273 (return 00493275), D3D9 slot106/+0x1A8.
  ESI is the owning receiver; device at +0x0C, bytecode array at +0x30, end at
  +0x34, per-variant shader object array at +0x60. It creates only for an empty
  output slot with non-null bytecode and stores the successful result.
- VS function 00846E70, call at 00846F63 (return 00846F65), slot91/+0x16C.
  Equivalent guarded creation with its object array at +0x50.
- 00CF83D0 and native selector tables determine the requested variant.

Current mod hooks call original CreateShader synchronously. Their pointer maps
hold reflection/stereo metadata, not owning reusable COM shader objects. Session
PS creation logs contain 147 requests / 144 hashes in A and 170 / 164 in B;
these 32-bit hashes alone do not prove identical effective bytecode/device.

The next concrete optimization target is preparing the **cold required variants
inside loading**, retaining the engine's own cache and driver thread ownership.
A second hash-only COM cache is not justified by these results: native slots
already cache successful first use. Safe loading entry, owning object lifetime
and the required variant set are not yet connected. No speculative cache,
asynchronous driver call, new native hook or synchronization bypass was added.
Combat verification is additionally blocked by the reproducible session failure.

## Crash, rollback, changes and validation

Both attempts record C0000005 at native memcpy 00E129EA, with the established
004E0E40 -> 00D163B0 -> 00E37C10 hash/buffer chain. A reads unmapped A1561184;
B reads D966AF14. The outer observed EBP chains differ; the corrupt source
producer and relationship to in-memory hooks remain unresolved. The VEH is
first-chance and no complete unhandled-exception dump is present. Process exit
and failed trial are established; terminal exception handling is not. No crash
fix, validation bypass or stability claim is made.

Persistent-log code from 7eb665a is retained in this branch. The only new code
change is the existing collector's baseline duration: its two-second PresentMon
warm-up precedes baselineQpc, so subtracting two again shortened the recorded
posture. Both A and B used the corrected full ten-second baseline schedule.
No new measurement tool, renderer/INI/hand change or additional deployment.

Final rollback verified original DLL and both INIs, local INI and EXE hashes.
Backup remains `perf-log-ab-20261026-105015`; previous backups are untouched.
A's manifest additionally reports an installed DLL hash change because MAIN
restored the DLL after process exit while ETW postprocessing was still active.
That did not replace code in a running process or during its usable samples;
the original invalid manifest is preserved. B rollback followed collector exit.

Offline: production and diagnostic x86 builds PASS; persistent-log regression
(10,000 serialized records, live flushed-line visibility, close/reopen) PASS;
existing metric unit tests 2/2 PASS; Present-readiness tests 8/8 PASS;
x86 actual-call-site forwarding/HRESULT/recursion/rollback regression PASS.
Aggregate CPU partitions and classified wait sums conserve each eight-second
window; GPU DMA union plus complement equals 8000ms. Raw traces/logs, private
shader/disassembly data and build binaries stay outside Git. Aggregate measured
numbers and source-file hashes are in the accompanying JSON.

Status: logging mechanism IN-GAME VERIFIED on the observed stationary prefix;
combat improvement NOT VERIFIED; stability FAIL; HMD ACCEPTED NOT OBTAINED.
No merge, no release deployment and no further test authorization requested.
