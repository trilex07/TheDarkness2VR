# Existing ETL: critical path and persistent-log candidate

Branch: `codex/perf-critical-path`, implementation based on `4665f73`.
No Steam launch, new capture, deployment, antivirus change, or merge was performed.
This supersedes the partial conclusions in `perf-combat-20261026.md`.

## Finding and correction

The main/render thread repeatedly opens and closes the diagnostic log synchronously.
In all ten worst combat frames its blocked stack reaches `Log -> ucrtbase ->
KernelBase!CloseHandle -> FLTMGR -> vlflt.sys`. The corresponding ReadyThread events originate in
`bdservicehost.exe (2568)`. The local driver metadata identifies vlflt as the
Bitdefender filter. This is direct evidence of file/filter waits on the critical
path, not an inference from low GPU utilization.

**242.798 ms across 124 waits in those ten frames** belongs to this Log path.
Across the entire combat window, **6410.166 ms / 2790 render-thread waits** end
with bdservicehost readying the thread. The full-window number is a wake-source
aggregate; only the ten-frame number has been individually attributed to Log
stacks. Do not equate all antivirus activity with these waits.

The candidate changes `src/d3d9.cpp` and adds `src/persistent_log.h`: open one
shared-readable stream during initialization, retain it, flush each complete
line, close after the final detach record and VEH removal. Existing serialization
is retained; readiness is atomic and checked again under the lock. No logging
queue, missing-line rate limit, fence removal, shader change, or resource-lifetime
change is introduced. This removes repeated file opens from **all** Log callers,
unlike the narrower Present-status deduplication in `4665f73`.

Expected effect: remove repeated open/close/filter round trips, including the
observed close-time filter blocking. Write/flush and
lock costs remain. The captured DLL predates `4665f73`; the incremental FPS benefit
over that commit is **not measured**. Status: IMPLEMENTED / OFFLINE VERIFIED;
IN-GAME VERIFIED and HMD ACCEPTED: NOT RUN. Not a 90 FPS claim or a crash fix.

## Clock alignment and usable interval

Private source: `%LOCALAPPDATA%/Darkness2VR/perf/20261026-093553`.
ETW has zero lost events/buffers; PresentMon's queried session has zero loss.
The original invalid manifest is preserved; its parsing/lifetime-check defects
are documented in the previous note. They do not invalidate the earlier events.

`xperf -a dumper -add_rawdata` supplies a raw QPC/relative-time pair:
`0x87d055fe3` ticks corresponds to `10000000800` relative nanoseconds.
At 10 MHz this gives trace origin **36357242587 QPC ticks**. The next two raw
events give the same origin. Convert with `ETL_us = (QPC - origin) / 10`.
PresentMon CPUStartQPCTime is milliseconds, so multiply by 10000 first.

- Stationary phase: **2.7312865–10.8712984 s**, actually 8.1400119 s, 534 frames.
- Combat phase: **10.8712984–45.8846039 s**, 35.0133055 s, 2208 frames.
- All intervals below are pre-crash. The final PerfClock preceding the exception
  maps to **91.742 s**, later even than ETL end (67.825 s).
- Log PerfClock markers occur every 120 native frames. They bracket, but cannot
  uniquely number, each PresentMon frame. No per-frame OpenXR timestamp provider
  was recorded. Five-second CPUSync summaries are not exact per-frame timings.

## CPU execution, blocking, and thread ownership

CSwitch + ReadyThread reconstruct disjoint scheduled/blocked/ready intervals.
`xperf -a dumper -exc_dpcisr` separates interrupt/DPC occupancy from execution.
Where a frame cuts through a running segment, interrupt placement yields a
small execution range rather than an invented exact value. Sampling period is
1000 us throughout, verified with `xperf -a profile -freq`.

TID **20756** is the EXE main/render thread (start VA E1CDAB, engine Present and
frame-loop stacks). During combat: **25.12037–25.12038 s actual CPU execution**,
0.40007 s interrupts while scheduled, **8.944315 s blocked**, **0.548059 s ready**,
and 0.000482 s unknown transient states. This is not a thread mostly waiting
for a frame-rate deadline. Its average execution alone is about 11.38 ms/frame.

TID **8708** starts in `nvd3dum.dll+unknown` (recorded VA 5C353384) and submits the
D3D9 context; 12.001 s scheduled, 22.247 s blocked, 0.765 s ready in combat.
8939 one-ms samples are in nvd3dum. It readies TID 20756 in 21087 UserRequest
waits totaling **1461.570 ms**. In the slowest frame it runs for 47.878 ms while
20756 waits in shader creation and transfer; these times overlap and are NOT
added. Private NVIDIA function names and internal synchronization-object handles
are UNKNOWN; the driver/event dependency is recorded, not its mutex name.

TID **33076** starts in `nvwgf2um.dll` (VA 510724CA): 1.169 s scheduled,
33.636 s blocked. Engine workers **6728,19720,8800,4228,26932,28280,31072** start
at EXE E1AE5B and mostly wait with WrQueue; their scheduled times are respectively
463,434,391,389,376,330,257 ms. No evidence identifies them as the dominant
blocking dependency. **17796** is dinput8 (416 ms scheduled); **5872** is
XAudio2_7 (328 ms). Remaining threads are included in the companion CSV, not
silently treated as render workers. No separate main/game thread is established.

Full combat render wait reasons: Executive 6500.420 ms, UserRequest 2397.399 ms,
WrResource 46.105 ms, WrPushLock 0.141 ms, WrAlertByThreadId 0.218 ms,
WrFastMutex 0.032 ms. Resource/mutex waits are present but not dominant.
Other wake sources include Steam (428.037 ms) and compositor (362.516 ms);
being the readying process does not by itself prove ownership of a contested lock.

CPU Sampled corroborates execution, independently of waits: render-thread EXE
5991 samples, kernel 7023, ntdll 3742, both d3d9 modules combined 2728,
wow64cpu 1873, nvd3dum 1237. These are sample estimates, not precise path durations.
910 samples hit the recorded ntdll `ZwQueryVirtualMemory+0x14`; source has guarded
memory inspection, but caller attribution for all these samples is UNKNOWN.
No validation check is removed on that evidence. The all-event butterfly's
percentages are deliberately not used as execution-time percentages.

## Five dominant identified blocking paths in the worst ten frames

All five below are TID 20756, duration is blocked time, and frequency is the
number of corresponding blocked segments. These partition waits; sampled CPU
execution and GPU activity must not be added to them.

| Path established by stack | Total ms | Waits |
|---|---:|---:|
| Log / CRT file close / FLTMGR / vlflt | 242.798 | 124 |
| Hook_CreateVertexShader or PixelShader / D3D9 / NVIDIA event wait | 51.859 | 18 |
| CaptureBackbufferToShared / reader or producer transfer / driver wait | 20.658 | 40 |
| Other nvd3dum wait stacks; private function UNKNOWN | 6.122 | 79 |
| vrclient/OpenXR-side stacks; exact API UNKNOWN in this classification | 2.142 | 69 |

An additional **4.873 ms / 70 waits** remains UNKNOWN; it is not hidden inside
an invented named path. The raw private per-frame exports retain wait and
readying stacks. xperf shifts a blocked thread's stack to its switch-in time;
the analysis joins that to the preceding wait, rather than incorrectly joining
an unrelated thread's stack at the switch-out timestamp.

Ready-stack examples are also present: at 33.477553 s, bdservicehost TID 4164
returns through `vsdk.dll -> fltLib!FilterReplyMessage+0x2E -> FLTMGR` and readies
20756. At 33.495187 s, NVIDIA TID 8708 readies it via
`nvd3dum -> KernelBase!ReleaseSemaphore+0x14`. The blocked counterpart includes
`WaitForSingleObject+0x12`. ETL module bases/checksums match the local PE exports;
Capstone confirms `CloseHandle+0x57` is the return after the NtClose import call.
This narrows the file stall to closing the per-line stream, not a guessed disk
bandwidth problem. The private NVIDIA semaphore identity remains UNKNOWN.

## Ten worst combat frames

All times except start are milliseconds. CPU excludes DPC/ISR; ready means
runnable but unscheduled. GPU is the union of the game's DMA start/stop spans,
not an additive sum of overlapping contexts. Full metrics, including PresentMon,
OpenXR-side blocked time and queue latency, are in `perf-worst-ten-20261026.csv`.

| ETL start s | Frame | CPU | Blocked | Ready | GPU DMA | No game DMA | Main blocked path |
|---:|---:|---:|---:|---:|---:|---:|---|
| 33.466727 | 69.202 | 13.204–13.214 | 55.036 | 0.433 | 3.647 | 65.555 | shader 26.600 + Log 14.381 + transfer 12.800 |
| 26.627739 | 59.168 | 17.904 | 40.728 | 0.517 | 4.330 | 54.838 | Log 38.380 |
| 33.975038 | 57.908 | 16.144 | 40.646 | 0.414 | 3.851 | 54.057 | Log 27.526; shader 6.652; transfer 5.561 |
| 15.066682 | 54.570 | 14.656 | 38.976 | 0.707 | 4.249 | 50.321 | Log 36.109 |
| 35.273821 | 52.263 | 19.484 | 31.963 | 0.495 | 3.717 | 48.546 | Log 24.119; shader 6.131 |
| 25.928234 | 43.065 | 15.238 | 27.134 | 0.480 | 4.039 | 39.026 | Log 26.139 |
| 18.407226 | 42.976 | 15.119 | 26.866 | 0.392 | 3.967 | 39.009 | Log 25.421 |
| 11.915774 | 37.962 | 12.209 | 25.353 | 0.398 | 4.388 | 33.574 | Log 23.846 |
| 33.535929 | 37.866 | 14.401 | 23.016 | 0.432 | 3.105 | 34.761 | Log 15.249; shader 6.886 |
| 44.507753 | 37.774 | 18.421 | 18.734 | 0.261 | 3.534 | 34.240 | Log 11.628; shader 5.055 |

CPU + interrupts + blocked + ready reproduces each frame within 2 us. The
classified blocked times sum to the independently reconstructed blocked time.
Present API durations for these frames are 0.477–0.837 ms. Recorded vrclient
blocked spans are 0.182–0.285 ms/frame: no evidence of a long xrWaitFrame deadline
causing these ten spikes. Longest gaps without game DMA range 17.678–43.278 ms;
the slowest frame's is 20.808 ms. A missing game packet is not proof the entire
GPU is idle: compositor and other processes may execute in that gap.

## GPU, compositor, stationary versus combat

Two game GPU contexts are observed: `...342acde0` is submitted by driver TID
8708 (D3D9 stacks), `...3444dde0` by render TID 20756 (D3D11 transfer path).
Both map to node 0. Same-frame live versus replay cannot be divided precisely
from these ETW packets: they share a context and no per-pass GPU markers were
recorded. Existing GPUFrameSplit logs are coarse elapsed timestamps, not busy
execution or exact matches to these ten frames. Preserve that limitation.

Combat game DMA union: **8.710 s / 35.013 s**; no game DMA: 26.303 s. Mean
queue-submit-to-DMA-start is **0.064 ms**, max 2.544 ms. In the ten worst frames
the mean is 0.025–0.239 ms. Work waits mainly before submission, not behind a
large posted GPU queue. 54777 matched game packets, zero unmatched DMA stops.

PresentMon baseline/combat mean frame: **15.239 / 15.851 ms**; p95
18.343 / 19.432 ms. GPUBusy: **3.350 / 3.511 ms**. Its GPUWait means
11.525 / 11.857 ms are NOT CPU thread wait. The game frame includes CPU
execution, file waits, driver work and submission gaps; GPU overlaps that work.
Average render CPU execution rises from about 10.84 to 11.38 ms/frame, blocked
time from 3.97 to 4.05 ms, while ready delay falls from 0.270 to 0.248 ms.
The 15.85-versus-3.51 difference is therefore not unexplained GPU rendering cost.

SteamVR compositor has 3148 DMA packets during combat (~89.9 packets/s; NOT
proof of 89.9 newly rendered application frames), DMA union 1.682 s and mean
submission delay 0.033 ms. SteamVR's retained session totals report 16469
presents, 31 dropped and 5889 reprojected. They cover a longer session and must
not be attributed solely to the 35-second combat window. Per-frame HMD display
and reprojection status for the ten selected frames is UNKNOWN.

Other recorded GPU activity includes DWM (1.187 s), ChatGPT (0.628 s), fpsVR,
and short unrelated graphics-test processes (about 4 ms each). This was not an
isolated machine. Their activity is not proof of causing the game's file waits;
measured render ready delay remains small. No process was terminated or changed
as part of this analysis.

## Crash: producer narrowed, cause unresolved

Read-only Ghidra on the existing project confirms EXE SHA
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
The preserved chain is memcpy `00E129EA` <- hash update `004E0E40` <- wrapper
`00D163B0` <- hash/relocation routine `00E37C10` <- `00D174B0` <- `00897800`
<- `00705F20`. The invalid source is `471AA1A2`, not a known hand-palette pointer.

New disassembly establishes upstream pointer construction: `00897837..44`
loads an encoded relative word, XORs it with the transformed input key, and
adds the encoded-word address. `0089786B` passes that result to the callback;
the caller supplies the encoded word at `0104B674`. Helpers `00632EC0` and
`006F4C30` transform bits of the input. `00D174B0` forwards to `00E37C10`, which
hashes the source directly when its relocation delta is zero, otherwise adjusts
relocation data before hashing. This is a native encoded-address/hash path,
not demonstrated enemy or hand geometry processing.

The exception record lacks runtime values of the encoded word, transformed key,
and caller input. Static disassembly cannot recover which value changed or who
changed it. Interaction with in-memory mod patches is possible but unproven;
a native engine defect is also not established. No protection/validation bypass,
native patch, or speculative crash correction is included. The VEH record is
first-chance; the user's self-exit report proves a failed session, not a complete
unhandled-exception dump.

## Offline validation, artifacts, next verification

- New x86 regression: persistent writable handle; live reader sees the flushed
  exception line; 10000 serialized records from four threads with no loss or
  interleaving; repeated close; new-session truncation. PASS.
- Present readiness: 8 tests PASS. Actual x86 call-site forwarding, HRESULT,
  recursion and rollback regression: PASS. Frame/CPU/GPU conservation: 10/10.
- Diagnostic and production DLL builds: PASS. No install or live A/B test.
- Diagnostic candidate SHA256: `664E6B7591A88C377DF63F323930AA30C32351AF44C3F55BC0FA19514ABC5106`.
- Production candidate SHA256: `7397E626B35628D262375244CC19310DDE0B7D7CC0D0A1722E44CD23F71FA9AB`.
- Both are kept under `dist/perf-persistent-log-20261026` (not committed).
  Installed DLL/INI hashes still match the preserved backup.

The reversible A/B is `4665f73` versus this commit with identical diagnostic
settings and scene. Verify actual per-line file-close wait removal, frame p95/p99, full
log visibility and unchanged stereo. Do not project the 242.8 ms wait total as
a guaranteed FPS gain. At most two controlled attempts; stop/rollback on error.
This document authorizes no launch. No new test is requested in this task.

## Reproduction and provenance

Existing tools only: xperf dumper (Thread provider
`3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c`, `-stacktimeshifting`, `-exc_dpcisr`),
SampledProfile provider `ce1dbfb4-137e-4da6-87b0-3f59aa102cbc`, DxgKrnl
`802ec45a-1e99-4b83-9920-87c98277ba9d`, and existing Ghidra CameraEvidence script.
WPA Precise aggregate export was cross-checked; raw scheduler events supply
per-frame partitions. Private derived JSON/CSV exports remain next to the ETL;
only aggregate frame/thread metrics are committed. Sampling estimates, scheduled
time, interrupt time, wait time and GPU unions are kept distinct.

Source SHA256: PresentMon CSV
`5c4f52f9f547c7f85e7c518aa4817443d2e225d1e7106ee658eb3b7c2dd00c9d`;
original manifest `b2b08ee6e9c91f1a4b23c736042846e47aa88ee44ee5fbee4a5f41dedd87d28d`;
ETW stats `bd90fb53f54f4fccb14c22ce3d46d77075d5a6de1e43653acc01ec4f132f696a`.
