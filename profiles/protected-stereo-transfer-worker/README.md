# Same-frame stereo transfer worker

Startup-only opt-in on the protected CPU profile. Merge the one Render key into
the effective INI, do not replace the user's configuration with this snippet.
Restart the game when changing modes:

- `0`: original serial transfer and original device flags (default).
- `1`: D3D9 producer polling plus D3D11 eye blit/read completion on a worker.
- `2`: serial transfer with the same D3DCREATE_MULTITHREADED creation flag as 1,
  for separating flag overhead from actual overlap in a controlled comparison.

Mode 1 overlaps left-eye transfer with right-eye D3D9 replay and right-eye
transfer with live-state restoration. The game, native resource operations,
recording, replay and original Present stay on their existing thread. This is
NOT a complete asynchronous native renderer or another frame of buffering.

The worker consumes retained mod-owned COM resources and an owned copy of the
eye constants. It never reads engine buffers, recorder vectors or mutable camera
globals. Only the main thread prepares constants and calls capture diagnostics.
The main thread flushes the D3D9 producer event before handoff. The worker polls
that event, draws with exclusive use of the D3D11 context, unbinds the shared
SRV, flushes/polls the reader event, then returns ownership. The main thread
joins before shared overwrite, any further D3D11/XR operation or resource
teardown. Early returns also join through a scoped lease. Failure latches an
overwrite prohibition until shared-resource teardown; a timeout never authorizes
reuse of a resource still in use.

The D3D9 MULTITHREADED flag is required for concurrent query polling and replay;
its overhead may offset the benefit. Mode 0 retains the original flags. The
worker code is pinned for process lifetime and idle workers own no COM resources;
no thread join occurs under DLL unload's loader lock.

Validation: production and diagnostic x86 builds PASS; actual x86 D3D9Ex/D3D11
interop test PASS for 400 eye transfers, matching left/right pixels under
concurrent D3D9 work, early-return join, failure latch and Reset/recreation.
Generic queue regression and the existing stereo transfer ordering tests PASS.
Run `test/build_stereo_transfer_worker.ps1` for the isolated 64x64 GPU test.
This test does not launch the game, Steam or OpenXR.

Runtime witness: `[StereoTransferWorker] admitted` plus completed-job lines
with worker TID and main join wall time. These scopes overlap and must not be
added. Compare frame times in the same game scene, not just worker duration.
The earlier log exposed roughly 1.3 ms/frame across BOTH interop fence scopes;
only part can overlap. No FPS improvement or 90 Hz acceptance is claimed.

Integration for MAIN: this branch builds from e5264e0 and retains the protected
present/input profile. Cherry-pick the integration commit only after baseline
comparison and headset acceptance. Do not use the generic queue to defer native
D3D9 API calls: the native HRESULT/resource-version contracts remain unproven.
