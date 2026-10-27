# Combined CPU call reduction candidate

Apply the Render section of on.overrides.ini to the approved full game INI,
and its Diagnostics section to Darkness2VR-diagnostic.ini. Preserve all other
protected controls/head/gestures settings. Restart for all three admissions.
These files are overlays, never complete replacement INIs.

- SamplerCapsCache removes repeated successful positive MaxAnisotropy queries:
  one GetDeviceCaps per thread/device/generation, with invalidation at HookDevice,
  Reset and ResetEx. Failed/zero queries retry; no cached native memory permissions.
- SmallRecordCopy replaces small recorder queries with exact-count self-RPM only
  after its guard probe passes. This still makes a syscall per eligible record;
  it does not reduce thousands of shader updates to zero. Rejection/failure uses
  the existing validation and prefix/zero fallback.
- AsyncLog moves ordinary file writing off the render thread. Failed activation
  retains the existing fallback; fatal-exception handling remains independent.

Expected startup receipts: SamplerCaps cache=1, SmallRecordCopy requested=1
admitted=1 guardProbe=1, and successful async logger activation. A failed
admission must be reported as fallback, not measured optimization.

This combines existing implementations, without changing native shader-buffer
ownership, constants, draw calls, source resolution or eye rendering. Runtime
benefit of the combination is unmeasured. It is not a 90-Hz acceptance claim.
Use off.overrides.ini and restart to restore the legacy algorithms, or restore
the saved complete DLL/INI installation. Keep EXE hash protection enabled.
