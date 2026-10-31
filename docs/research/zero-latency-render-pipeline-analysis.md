# Zero-Latency Render Pipeline — Dependency Map & Analysis (SIDE)

**Branch**: `side/zero-latency-render-pipeline`
**Task**: Reduce D3D9/NVIDIA driver serialization and CPU frame time by improving execution overlap, without adding frame latency.
**Scope**: SubmitMode 4 (VR_NATIVE_STEREO), zero-copy shared capture path (`ZeroCopy=1` is set in the active `Darkness2VR.ini`), render thread only.
**Evidence base**: `src/d3d9.cpp` (render thread), active config `Darkness2VR.ini` (SubmitMode=4, StereoMode=1, ZeroCopy=1). No runtime testing performed by SIDE (offline only; MAIN validates on Steam).

---

## 1. Exact dependency map (per game frame, render thread)

`VrFrame(dev)` (called from the Present/frame boundary) runs, in order:

1. `RenderEyePairFromReplay(dev)` — both eyes are built from the recorded frame stream **before** submission.
   - Sets `g_eyePairReady = eye0Ok && eye1Ok` and `g_eyePairFrameId = g_trackedFrame.id`.
2. `SubmitXR(dev)` — `havePixels = g_eyePairReady && g_eyePairFrameId == g_trackedFrame.id`, then writes both eye textures to the OpenXR swapchains and calls `xrEndFrame`.
3. `BeginTrackedFrame()` — `xrWaitFrame` + `xrBeginFrame` + `xrLocateViews` for the NEXT frame. (**Not moved** — per task constraint.)

`RenderEyePairFromReplay` serial order (all on the render thread):

| Step | Operation | Thread/Queue | Produces | Consumed by |
|------|-----------|--------------|----------|-------------|
| A | `CaptureBackbufferToShared(dev, slot0)` | D3D9 (render thread) | left-eye pixels into **shared slot0** | step B (D3D11 read of slot0) |
|   ├─ `WaitSharedReader(slot0)` | D3D11 fence spin | — | waits for PREV FRAME's left-eye D3D11 blit on slot0 (between-frame) | |
|   ├─ `StretchRect(backbuffer -> shared slot0)` | D3D9 GPU | shared slot0 = left eye | step B |
|   ├─ `Issue(D3DISSUE_END)` + `GetData(D3DGETDATA_FLUSH)` spin | D3D9 fence | — | guarantees copy done before backbuffer is overwritten (step C) |
| B | `RenderStereoEyeD11(0)` | D3D11 (g_ctx11, async) | **eyeCache[0]** = left eye | `SubmitXR` CopyResource |
| C | right-eye `ReplayStream(dev, 1, sp1)` | D3D9 (render thread, CPU-heavy) | right-eye pixels into backbuffer | step D |
| D | `CaptureBackbufferToShared(dev, slot1)` | D3D9 (render thread) | right-eye pixels into **shared slot1** | step E (D3D11 read of slot1) |
|   ├─ `WaitSharedReader(slot1)` | D3D11 fence spin | — | **WAITS FOR STEP B's left-eye D3D11 blit** (WITHIN-FRAME serialization) | |
|   ├─ `StretchRect(backbuffer -> shared slot1)` | D3D9 GPU | shared slot1 = right eye | step E |
|   ├─ `Issue` + `GetData(D3DGETDATA_FLUSH)` spin | D3D9 fence | — | copy done before restore overwrites backbuffer | |
| E | `RenderStereoEyeD11(1)` | D3D11 (g_ctx11, async) | **eyeCache[1]** = right eye | `SubmitXR` CopyResource |
| F | restore live color + `liveEnd.Apply(dev)` | D3D9 (render thread) | backbuffer restored for next game frame | — |
| G | `SubmitXR`: `WriteTextureToSwapchain` (CopyResource eyeCache -> swapchain) x2 + `xrEndFrame` | D3D11 (in-order) + OpenXR | both eyes to compositor | HMD display |

**Cross-frame head pose**: the pose used for step A/C matrix parameters was acquired by the previous frame's `BeginTrackedFrame` (`xrWaitFrame`). That is the normal one-frame OpenXR pose latency, not additional latency introduced by this task.

---

## 2. Serialized unnecessarily: the left-blit / right-capture dependency

Steps B and D share a **single** shared capture surface (`g_sharedSurf9` / `g_sharedSRV11`):

- Step B reads slot0's shared SRV (D3D11) into eyeCache[0] — submitted asynchronously to `g_ctx11`.
- Step D writes slot0's shared surface (D3D9 `StretchRect`, overwriting the left eye with the right eye).

Because D3D9Ex and D3D11 run on separate queues (comment at `src/d3d9.cpp:2570`), there is **no implicit GPU ordering** between step B's D3D11 read of the shared surface and step D's D3D9 write to the same surface. The existing code therefore inserts `WaitSharedReader()` at the start of step D (line ~2560), which:

1. `g_ctx11->End(g_sharedFence11)` + `g_ctx11->Flush()`
2. spins `while (GetData(...) == S_FALSE) SwitchToThread();` until the D3D11 GPU reaches the fence — i.e., until **step B's left-eye blit has completed reading the shared surface**.

**This is the avoidable render-thread wait.** The right-eye capture (step D) blocks on the left-eye producer fence (step B) purely because both reuse the same shared surface. The left-eye D3D11 transfer and the right-eye capture are otherwise independent (different destination resources: eyeCache[0] vs backbuffer→slot1).

The D3D9 fence-flush spins inside `CaptureBackbufferToShared` (after each `StretchRect`) are **retained required synchronization**: step A's copy must complete before step C overwrites the backbuffer, and step D's copy must complete before step F restores the backbuffer. Those are not removed by this change.

The `WaitSharedReader` calls that wait for the **previous frame's** blit (between-frame, at the start of steps A and D) are retained: a slot is reused a full frame later, so that blit is normally long done; those spins are between-frame, not within-frame.

---

## 3. Scheduling improvement: double-buffer the shared capture surface

**Change**: give each eye its own shared capture slot:
- slot 0 = left eye: capture (step A) → D3D11 blit reads slot0 SRV → eyeCache[0]
- slot 1 = right eye: capture (step D) → D3D11 blit reads slot1 SRV → eyeCache[1]

Because the left-eye D3D11 blit reads slot0 and the right-eye capture writes slot1, they touch **different resources**. The cross-device conflict disappears, so:

- **Removed dependency**: right-eye capture (step D) no longer waits on the left-eye producer D3D11 fence (step B) within the frame. `WaitSharedReader(slot1)` now only waits for the previous frame's right-eye blit on slot1 (between-frame).
- The left-eye D3D11 transfer already overlaps the right-eye replay CPU work (step B submitted async while step C runs on the CPU); double-buffering removes the *render-thread fence wait* that could otherwise stall between them.
- **Zero added frame latency**: both eyes are still captured and submitted from the same game frame (`g_eyePairReady`/`g_eyePairFrameId` unchanged). `xrWaitFrame` is not moved. No cross-thread D3D9 calls (all D3D9 stays on the render thread). No synchronization disabled (per-slot D3D9 flush fences and D3D11 reader fences retained).
- **Reuses existing infrastructure**: shared surfaces (now x2), shared fences (now x2 per slot), frame context, `CaptureBackbufferToShared`, `RenderStereoEyeD11`, `SubmitXR`, `g_uploadSRV` repurposing (now per-slot SRV).

**CPU time saved (offline estimate)**: removes the render-thread `WaitSharedReader` spin-wait on the left-eye producer fence before right-eye capture, every frame. In the common case the GPU blit finishes during the CPU replay (spin already ~0), so average savings are small; the meaningful saving is removing the **worst-case render-thread stall** (bounded spin, up to the 2s fence timeout) and one cross-device D3D9↔D3D11 serialization point per frame. The dominant same-frame-stereo cost (right-eye `ReplayStream`, reissuing all draw calls) is inherent to SubmitMode 4 and is not touched (AFR was previously rejected; see `FAILED-APPROACHES.md`).

---

## 4. Correctness argument

- **Same frame produces both eyes**: `g_eyePairReady = eye0Ok(slot0) && eye1Ok(slot1)`, `g_eyePairFrameId = g_trackedFrame.id`, unchanged. Submission still requires both eyes from the same frame.
- **No premature shared-resource access**: each slot has its own D3D9 producer fence (`Issue` + `GetData FLUSH`) guarding its `StretchRect` write, and its own D3D11 reader fence (`WaitSharedReader`) guarding reuse. Slot0 is written by left capture and read by left blit; slot1 by right capture and read by right blit. No slot is written by one eye while read by the other.
- **D3D11 in-order submission preserved**: all D3D11 commands (left blit, right blit, `SubmitXR` CopyResource x2) go to `g_ctx11` immediate context in submission order, so `CopyResource` reads a complete `eyeCache`. Verified by existing code path (unchanged).
- **No unsafe cross-thread D3D9**: all D3D9 device calls remain on the render thread; only D3D11 query/fence polling is used for synchronization.
- **Failure degrades safely**: if a slot fails to initialize (CreateTexture/OpenSharedResource/CreateQuery), `EnsureSharedCapture` returns false for that slot, `zeroCopy` becomes false, and the frame falls back to the CPU `CopyFrame` path / drops submission (`g_eyePairReady == false`). No crash, no corrupt right eye.
- **Reset/shutdown**: `ReleaseSharedCapture()` now releases both slots; called from `Hook_Reset`/`Hook_ResetEx` as before.

---

## 5. What was deliberately NOT changed (and why)

- `xrWaitFrame` / `xrBeginFrame` / `xrEndFrame`: not moved or reordered (task constraint). Pose latency unchanged (one frame, pre-existing).
- Right-eye `ReplayStream`: not offloaded — D3D9 device calls are render-thread-only and not safe to move to a worker thread without a multithreaded device; AFR (alternative) was previously rejected.
- Snapshot/restore (`liveEnd.Capture`, `BackupLiveColor`, `SeedRightEyeBackground`, `liveEnd.Apply`): retained; not a confirmed significant critical-path cost relative to the fence serialization, and changing them risks right-eye/UI regression.
- D3D9 fences not replaced with D3D11 fences; required synchronization not disabled (per task constraints).

---

## 6. Offline validation performed by SIDE

- Read `src/d3d9.cpp`: `RenderEyePairFromReplay` (~2643-2781), shared capture (`ReleaseSharedCapture`/`WaitSharedReader`/`EnsureSharedCapture`/`CaptureBackbufferToShared`, ~2483-2579), `RenderStereoEyeD11` (~1967-2043), `SubmitXR`/`WriteTextureToSwapchain` (~3921-3919), `VrFrame` (~4515-4614), `Hook_Reset`/`Hook_ResetEx` (~4440-4513), config (`g_zeroCopyEnabled` default 0, ini `ZeroCopy=1`).
- Confirmed `ZeroCopy=1` and `SubmitMode=4` in the active `Darkness2VR.ini` → shared-capture path is on the critical path.
- Confirmed all shared-capture call sites are contained in `d3d9.cpp` (`RenderEyePairFromReplay`, `Hook_Reset`, `Hook_ResetEx`).
- Build: offline `cl` compile of `src/d3d9.cpp` (MSVC x86) to verify compilation; no Steam/runtime testing (SIDE constraint).

---

## 7. Next steps (MAIN)

- Review the diff, then build via `build.ps1` and install (MAIN only, Steam exclusive).
- In-game test: SubmitMode=4, StereoMode=1; verify both eyes render, no right-eye regression, hands/tentacles/UI intact, no new frame latency (pose freshness), stable frame times.
- If `WaitSharedReader` spin was measurable, confirm reduced render-thread wait via `d3d9.log` `[StereoPerf]` / ETW traces.
- Rollback: `build.ps1 -Install` creates `d3d9.dll.off-<timestamp>` backups; revert branch if regression.
