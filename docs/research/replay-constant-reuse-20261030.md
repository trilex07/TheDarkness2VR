# Replay float-state reuse candidate

`ReplayConstantReuse=0` is independent and OFF by default. It suppresses a
whole float packet only after every row has already reached D3D_OK with the
same final eye-adjusted bits in this replay. No raw/live-eye or cross-frame
values, pointers, native resource ownership, page permissions or shader
compilations are cached. Storage is fixed, owned CPU memory (about 9 KiB).

Reset knowledge at every stream, opaque command/vendor state and StateBlock.
Invalidate VS c0-c3 at each DIP because the tentacle scope writes outside the
float forwarder. Diagnostic readback draws clear both banks. Capture bypasses
the filter. Failed/throwing/reentrant calls do not establish residency; SEH
aborts reuse for the rest of that stream. Exact equality includes NaN payloads
and signed zero. Other packets retain their original range and argument order;
there is no splitting, new synchronization, readback or driver query.

The existing ReplayUploads receipt counts actual driver calls downstream of
the filter. ReplayConstantReuse adds skipped-packet counts in the same reporting
window. These are not FPS or critical-path timers.

Reference reviewed: DXVK D3D9DeviceEx::SetShaderConstants first records StateBlock
writes separately, then updates owned state and avoids dirtying unchanged
constants. Our implementation is independent and confined to immutable replay;
it does not replace the native backend or copy DXVK source.
https://github.com/doitsujin/dxvk/blob/master/src/d3d9/d3d9_device.cpp
https://github.com/doitsujin/dxvk/blob/master/LICENSE

Validation: actual-source replay regression fails on c7db4a53 (512 -> 512)
and passes with this change (512 -> 2), with all 256 draw observations identical.
That is a controlled repeated-packet fixture, not a game capture reduction or
an estimated millisecond saving. Both full x86 builds, 192-stream float-loop
production/diagnostic tests, 120036 differential/edge checks, binding, projection,
live texture/depth, StateBlock and VB/IB/UI regressions pass. No game deployment
or headset validation. The principal remaining combat bottleneck and a saving
of 1 ms/frame are not established by this candidate. Steam retains 21e9b5b5.

For the next consolidated candidate, copy the current accepted diagnostic
profile, change only ReplayConstantReuse to 1, retain ReplayBufferVersions=0
and DirectRecordCopy=0. Compare existing ReplayUploads calls, CPU wall time,
stereo, shadows, UI, hands and tentacles together. Revert this independent
switch on instability, new artifacts or worse frame time. Do not promote
this option solely from offline call-count reductions.
