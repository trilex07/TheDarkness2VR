# Current CPU and engine optimization audit — 2026-11-01

## Evidence from the installed gaze NIS candidate

The current game log reads `NativeSubmissionFilter=1`, but repeatedly reports
`[NativeSubmission] enabled=0 denied=0x10` and zero forwarded/skipped VS and
PS packets. `NativeBinding` likewise reports zero forwarded/skipped calls.
Thus this installed candidate receives **no CPU saving** from either native
filter. The logged 0x10 bit is `Epoch::UncontrolledMutation`.

The source has four possible origins for this bit: a live
`SetSoftwareVertexProcessing` call through vtable slot 77, failure to
install that vtable hook, a recorder mutation during a native driver callback,
or a zero thread identity. The existing log does not distinguish them.
`PatchVTable` can return null if the image-write policy or `VirtualProtect`
refuses the slot. The exact cause therefore remains unproven. Do not remove a
denial without tracking its mutation boundary: mixed vertex processing may
change the shader constant bank.

The current log shows roughly 90 world-bearing XR layers/s in the recently
tested scene. That is compositor cadence, not native simulation FPS, and that
scene did not reproduce the user's worst drops. NIS gaze copy-base shortened
measured NIS GPU work, but it does not reduce the game's CPU geometry, resource
or simulation work.

## Scale and next gate

An older protected outdoor trace measured 20.429 ms mean app interval and
17.925 ms scheduled-running render time per frame, versus 3.818 ms GPU Busy.
That establishes an important CPU problem in that earlier build and scene,
not the current build's exact deficit. Older combat sampled stacks attributed
4.244 ms/frame to native EXE leaves and 3.437 ms/frame to an ambiguous d3d9
module; these exposures include necessary work and predate mode 4.

The strongest concrete mod-side opportunity is to make native float/binding
elision eligible without losing the ownership safety contract. Earlier analysis
estimated about 1.8 ms/frame of exposed float/binding submission cost, only
some of which could be redundant. First add a one-shot reason log distinguishing
slot-77 hook installation failure from an actual software-processing call,
including HRESULT and thread at the latter boundary. Then collect a current
worst-scene trace and count duplicate submissions. Correct only the proven
cause, retain the fail-closed policy, and compare native app intervals and
visible stereo after admission. A stateblock lifetime prerequisite exists but
is not wired into admission and saves zero runtime time as shipped.

The native resource/LOD dependency routines are smaller distributed costs in
older samples and have no proven safe per-object owner or decoration
classification. Lowering global LOD or resource priority by gaze would risk
streaming and visible geometry without established CPU benefit. No major
engine-CPU improvement is claimed from the current gaze implementation.

Sources: `src/submission_epoch.h`, `src/d3d9.cpp`,
`docs/research/protected-outdoor-critical-path-20261026.md`,
`docs/agent-workflow/handoffs/HANDOFF-major-cpu-optimization.md`,
`docs/research/outdoor-cpu-packets-20261026.md`,
and the local `%LOCALAPPDATA%/Darkness2VR/d3d9.log`.

## CPU gate candidate

The next source revision converts an intercepted software vertex-processing
transition into a full epoch invalidation and a scoped native bypass. A prior
constant/binding certification cannot survive a bank switch, and callbacks
inside the driver call cannot reuse the cache. Hook-install failure, foreign
thread, recorder callback, opaque StateBlock and interface escape still deny
admission. Sparse `[NativeSubmissionGate]` status reports the denial-source
bits (1 recorder callback, 2 slot-77 hook failure, 4 missing thread), software
transition count and the first call's thread/value/HRESULT.

Offline boundary, epoch and float-cache tests pass. The protected x86 DLL
passes the profile gate. This candidate has **no measured CPU saving** until
the user runs it and `skipped` counters rise. The usual PDB-enabled build
failed at link with a Visual Studio `MSPDB140.DLL` version mismatch in both
installed toolsets. The candidate was compiled from the same source with
`D2VR_MATCHED_CAPTURE=1`, x86 `/O2`, and a linker MAP, without PDB generation.
