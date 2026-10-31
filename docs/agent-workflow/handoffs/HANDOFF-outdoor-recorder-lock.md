# HANDOFF: outdoor live-phase CPU (side/outdoor-recorder-lock)

Base: 75432ae3. Independent of side/present-overlap and side/pipeline-variant-warmup.
Offline-only validation. No Steam or PSVR2 validation has been done.

## What limits outdoor FPS (existing evidence, no new profiling)

From the d3d9.log mode-4 windows, fast quartile vs slow quartile:

| Metric | Fast quartile | Slow quartile |
|---|---|---|
| Frame time | 11.19 ms | 13.51 ms |
| VS constant uploads per frame | 1478 | 2777 |
| PS constant uploads per frame | 1197 | 1965 |
| Pair boundary change | baseline | about +0.2 ms |
| Right-eye producer wait | baseline | lower |
| xrWaitFrame in slow windows | | about 0.02 ms |

Read together, these rule out GPU execution and synchronization as the limit.
The sustained outdoor slowdown is CPU work in the live phase that scales with
D3D9 call volume. The earlier ETW run agrees: outdoor RUN time rose by 5.35 ms
while blocked time stayed about the same.

The ETW run (perf/20261030T205050Z-eab0975b, combat) splits the mod's own
live-phase cost as follows, about 2.96 ms per frame in total:

| Item | ms per frame |
|---|---|
| Shadow::Stage | 0.434 |
| Recorder lock, release | 0.243 |
| Recorder lock, acquire | 0.121 |
| Native_SetVSConstF | 0.273 |
| RecPushLocked | 0.223 |
| ApplyShiftToUpload | 0.108 |
| MakeEyeParams copy | 0.103 |
| Input total | about 0.3 |

Of the lock time, AutoBoost bookkeeping accounts for 0.191 ms:
RtlAbPostRelease 0.162 and RtlpAbFindLockEntry 0.029.

Shadow::Stage stays as it is because skipping it was rejected earlier for UI
correctness. The rest of the outdoor cost is the native engine and the D3D9
runtime and driver.

## Change

1. `src/recorder_lock.h` adds a three-state mutex (0, 1, 2) that replaces the
   SRWLOCK in `recorder_sync.h`.
   - Uncontended lock and unlock each cost one interlocked operation, with no
     AutoBoost ownership tracking.
   - Under contention it spins briefly (bounded, at most 128 pauses) and then
     blocks on a lazily created kernel semaphore.
   - Depth, held, DriverScope and RecoverReplayException semantics are
     unchanged, and only the five call sites change.
   - The lock is constant-initialized (verified with constinit).
2. A new `EyeParamsRef(eye)` accessor returns the per-frame StereoParams cache
   by const reference, so each upload no longer copies about 170 bytes.
   - It is used in the VS constant hook (d3d9.cpp) and the PS constant hook
     (stereo_replay.inl).
   - Mode 2 still forces separation to 0. A copy is made only in a frame where
     the cached params still carry a separation.
   - When the frame cache is invalid, it rebuilds via BuildEyeParams exactly as
     MakeEyeParams does.

Stereo math, the order of commands sent to the driver, OpenXR ordering,
latency, hands, tentacles and UI are unchanged.

## Expected saving

These are estimates, not measurements:

| Scene | Lock | MakeEyeParams | Total |
|---|---|---|---|
| Combat | about 0.2–0.3 ms | about 0.1 ms | about 0.3–0.4 ms |
| Outdoor (call volume ×1.6–1.9) | | | about 0.45–0.65 ms |

This alone will not reach a native 90 FPS outdoors.

## Tests

| Test | Result |
|---|---|
| `test/recorder_lock_test.cpp` | 22 checks pass, clean under g++ -Wall -Wextra -Werror, ASan/UBSan and TSan |
| `tools/test_recorder_lock_contract.py` | PASS |

The lock test is portable and stresses exclusivity with 2–16 threads, lost
wakeups and the DriverScope release/resume pattern. MAIN should also run
`test/build_recorder_lock.ps1` (MSVC x86) to exercise the Win32 semaphore
path.

## MAIN checks

1. Build the DLL with build.ps1. The VM cannot run MSVC.
2. Run the existing tests that include `recorder_sync.h`:
   - `test/live_stateblock_sync_test.cpp`
   - `test/native_submission_boundaries_test.cpp`
   - `test/native_constant_source_test.cpp`
   - `test/live_record_status_test.cpp`
   - `test/projection_history_test.cpp`
   - `test/pixel_shader_metadata_test.cpp`
   - `test/frame_resource_refs_test.cpp`
   - `test/replay_buffer_versions_test.cpp`
   - `test/up_geometry_capture_test.cpp`
   - `test/stateblock_recording_test.cpp`
   - `tools/frame-boundary-simulator/replay_constant_window_test.cpp`
   - `tools/frame-boundary-simulator/small_record_copy_test.cpp`
3. In Steam, compare an outdoor area before and after on frame time and on
   RecLockGuard in the CPU sample. Expect lower live-phase RUN time and no
   visual change.
