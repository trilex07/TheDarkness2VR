# Render worker prototype

Base: e5264e0. This is an isolated implementation experiment, not an enabled
Darkness II backend. No existing hook, profile, deployed DLL or game file changes.

`src/render_command_worker.h` implements a bounded producer/consumer queue. The
backend is constructed, called and destroyed on one worker. Producer work can
overlap execution; a full queue sleeps instead of spinning. Accepted packets are
drained at shutdown. Payloads must own their data. An acceptance ticket is NOT a
successful graphics API return; failures are reported at completion barriers.
Already accepted packets are still executed after a failure, while new admission
is refused. This is an experiment contract, not native D3D9 error equivalence.

Run x86 tests from this checkout:

    .\test\build_render_command_worker.ps1
    .\test\build_render_command_worker.ps1 -D3D9Reference

The first exercises 20,000 ordered packets, copied payloads, producer progress,
backpressure, draw/update/draw versions, error/exception propagation, startup
failure and teardown. The second creates a real system D3D9 REF device and hidden
64x64 window on the worker, runs 1001 draws, a synchronous getter barrier and a
Reset, then destroys everything on that thread. It never loads the proxy or
touches Steam/OpenXR. It requires access to the software reference device.

Verified 2026-10-27: both x86 /O2 /W4 /WX tests PASS. The restricted account
returned D3DERR_NOTAVAILABLE for REF; the same test passed under the owner account.
NULLREF was also attempted and returned D3DERR_INVALIDCALL; the final smoke uses
REF explicitly. No hardware performance, pixel equality or headset test performed.

## Game integration is deliberately incomplete

Do not call the current ReplayStream from this worker. Existing frame COM
retention preserves interface lifetime, not the version of mutable VB/IB/texture
contents. Resource Lock/Unlock and getters are not comprehensively intercepted.
The stream also uses shared mutable globals and contains an oversized DrawUP
fallback carrying a source pointer. These are not worker-owned packets.

The adapter must cover creation, QueryInterface aliases, Lock/Unlock, transfers,
getters, state blocks, queries, Reset and destruction before permitting native
submission to progress concurrently. Writes must be queued as owned snapshots or
preceded by a dependency barrier. Native caller HRESULT behavior must be proven
before any method can return acceptance ahead of actual D3D9 completion; otherwise
that call must remain synchronous. Window/device thread rules must be preserved.

MAIN integration: no cherry-pick into the active renderer as a performance fix.
Reuse the queue only inside an adapter with those contracts implemented and
tested. Keep the accepted SmallRecordCopy/SamplerCapsCache/AsyncLog baseline.
The worker is not connected to that baseline and supplies no FPS improvement yet.
