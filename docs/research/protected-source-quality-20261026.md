# Protected source-quality candidate

Status: IMPLEMENTED / OFFLINE VERIFIED. Steam and PSVR2: NOT RUN.
Base: 85eea69. Default protected profile, renderer calibration and source quality
remain unchanged. The candidate does not enable legacy ProductionResolution.

## Focused correction

The old successful sizing path intercepted the EXE's GetClientRect IAT entry,
which is forbidden by the protected architecture. Merely requesting a bigger
D3D9 backbuffer had left the final source RT0 at 1920x1071.

This candidate instead installs a process-local implementation trampoline on
USER32!GetClientRect, using the existing MinHook dependency. It changes the
returned dimensions only for the exact verified native call's return address
C96474 (call C9646E) and the HWND acquired by CreateDevice/CreateDeviceEx before
HookDevice. Other callers, windows, failed API returns and null outputs are
forwarded unchanged. BOOL and last-error are retained, and the original API is
called exactly once. A changed Reset window clears the previous size witness.

Admission requires the existing exact EXE SHA/PE identity, original six-byte
call instruction and IAT pointer equal to the USER32 export. The executable,
IAT, native/static vtables, COM objects and resource descriptions are not
modified. The target and conservative decode/write span must be executable
external MEM_IMAGE memory outside the guarded EXE; module owners are pinned.
There is no hook teardown from DllMain or hot configuration edit.

The engine still receives one coherent size through its existing manager;
there is no independent resizing of individual render/depth/postprocess targets.
The historical engine sizing evidence motivates this candidate. Correct
upstream dependency sizing, viewport/UI behavior and image quality still need
runtime validation; the new output-RT0 witness alone does not prove all of them.

## Bounded modes

`Render.SourceQuality` is startup-latched in the protected diagnostic build:

- 0: OFF (default; no new hook or hot-path atomics).
- 1: native client size, safe control with the observer active.
- 2: 1.125 times each axis, approximately 26.6% more source pixels.
- 3: 1.25 times each axis, approximately 56.3% more source pixels.

Modes 2/3 preserve aspect ratio within two-pixel rounding. Plans beyond a
3840-pixel axis or 8,294,400 pixels are refused rather than silently cropped or
downgraded. Invalid values disable this feature. No FOV, shader, sharpening,
stereo separation, XR dimensions or synchronization changes are included.
The standalone overrides file selects mode2 for a separately authorized test;
it is not merged into the accepted rendering profile automatically.

At a 1920x1071 real client size the policy requests 2160x1204 (mode2) or
2400x1338 (mode3). These numbers are **requests, not proven gameplay RT sizes**.
The existing actual RT0 GetDesc at the stereo source copy logs outputRT0,
requested dimensions, exact match and request count. It does not relabel the
D3D11 eye-cache or OpenXR swapchain size as source resolution, and it does not
prove the resolution of earlier G-buffers. Require a loaded-gameplay matched
capture and comparable frame time before accepting an increased mode.

## Offline verification

- `build-source-quality.ps1`: PASS. Real x86 external-DLL trampoline fixture,
  WINAPI ABI, one original forward, exact caller/window, unsuccessful/null calls,
  LastError preservation, window replacement, repeated calls, and EXE target
  refusal. The test's complete .text is unchanged after interception.
- Deterministic dimension sweep: native and increased mode bounds, overflow,
  cost rejection, aspect and invalid mode checks PASS.
- DiagnosticCapture DLL and ordinary production DLL both build PASS.
- Existing protected-profile inheritance/unsafe-hook regressions: 2 PASS.
- No Steam read/write/deployment, game launch or physical headset test by SIDE.

## Rollback and next MAIN gate

Keep the established last-working DLL/INI backup. SourceQuality=0 on the next
fresh process disables this candidate; MAIN may also restore the prior DLL and
all INIs after normal process exit. Do not uninstall a live trampoline or raise
resolution while a user game is active. First compare mode1 control with mode2
under the same protected profile and scene, verify actual output RT0 and upstream
buffers using existing captures, and reject the increase if frame cost is too
high. No FPS/clarity improvement or HMD acceptance is claimed offline.
