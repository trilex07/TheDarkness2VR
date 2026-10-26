# Protected Present: event-only recovery

Base: `2e70cc4`. Candidate is offline verified; Steam and PSVR2 are NOT RUN.

## Defect and bounded correction

The first outdoor attempt stopped advancing XR frames before its timed capture.
Native Present continued, returning `S_PRESENT_OCCLUDED` after 8181 `D3D_OK`
returns. XR was last observed in SYNCHRONIZED (3), with zero submitted layers.
The headset woke later, but the only `PumpXR` call was in `BeginTrackedFrame`,
which was unreachable while the protected observer's warm/admission bit was
cleared. This explains the missed subsequent runtime events. The originating
desktop occlusion/focus change is not proven by these logs.

The correction adds an event-only branch to `ImagePresentBefore` when frame
admission is closed. It calls the existing `PumpXR` unchanged. It does not call
`GetDevice`, take a COM reference, enter a capture witness, replay commands, or
call `xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`. Existing session begin/stop and
input/reference-reset handling remains in `PumpXR`.

The branch requires the protected observer and image guard, enabled OpenXR,
initialized non-null XR instance/session, a qualified native outer Present,
the observer's verified current thread, and no observer fault or Reset. The
observer checks/claims that thread before invoking the callback. Its callback
and external implementation modules are already pinned; the new branch retains
no object pointers. XR instance/session are the existing process-lifetime
render-owned globals. Nested overlay calls, unrelated callers, foreign threads,
failed Reset, and uninitialized/disabled XR cannot poll through this branch.

The ready path is unchanged and still pumps through `BeginTrackedFrame` once.
An occluded return still clears readiness. A later real `D3D_OK` only rewarms
the observer; rendering resumes on the following qualified Present. Event
polling neither changes that rule nor proves that the desktop has recovered.
It does not force focus, restore a window, or solve an outdoor FPS problem.

## Offline verification

- `tools/frame-boundary-simulator/build-xr-event-recovery.ps1`: 117 checks PASS.
  Compiles the actual `ImagePresentBefore`, `ImagePresentAfter`, `PumpXR`, and
  `ApplyBoundaryDecision` bodies with the existing external native DLL fixture.
  The XR event queue and GPU `VrFrame` leaf are deterministic fakes. Covers
  `D3D_OK -> occluded -> VISIBLE/FOCUSED -> D3D_OK`, single ready-path polling,
  zero closed-admission frame/device calls, session stop/restart, reference
  changes, disabled/uninitialized handles, Reset, unqualified overlay callers,
  and foreign-thread fault refusal. It is not a headset test.
- `tools/frame-boundary-simulator/build-image-present-observer.ps1`: all five
  modes PASS, including forwarding ABI/private methods, 1000 lifetime cycles,
  Reset, overlay recursion, HRESULTs, EXE target refusal and thread-change
  refusal; fixture EXE text unchanged.
- `tools/frame-boundary-simulator/build.ps1`: 2,327,551 assertions, zero failures;
  50,000 synthetic and 25,000 timing cases PASS.
- `build.ps1 -DiagnosticCapture` and `build.ps1`: PASS.

No INI, renderer parameters, native gameplay hook, installed file, or main EXE
write path changes. Rollback is the parent revision `2e70cc4`; no migration.
The locally preserved diagnostic DLL SHA-256 is
`69815FBA624E4166FE14C4D5D67D34B71BBCE375911889E3D05E156E85D786F2`.
Binary build outputs and private runtime logs are not committed.

Live acceptance requires a separately authorized controlled session proving
that VISIBLE/FOCUSED events keep progressing while desktop Present is occluded,
then that a real `D3D_OK` restores the ordinary stereo/layer path. No live result
or performance improvement is claimed here.
