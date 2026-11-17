# Movement-stick range calibration for Pico reports (2026-11-17)

Status: IMPLEMENTED_NOT_VERIFIED in a headset. Offline tests/build only; not installed or released.

The player reports slow forward travel at full left-stick north, with normal
strafe speed, across multiple games. This motivates an optional range correction;
we do not yet have that controller's raw samples proving a reduced north maximum.
This setting addresses reduced stick amplitude, not every possible locomotion bug.

## Configuration

In the mod's main INI (WonderlandsVR.ini, Darkness2VR.ini or Binaries/SingularityVR.ini):

```ini
[Input]
MoveStickOuterDeadzonePercent=10
```

0 is the default and preserves the previous input exactly. 10 reaches full speed
at 90% radial deflection; 20 reaches it at 80%. Supported range is 0..40.
Start at 10 and increase only when raw samples show it is necessary. This is an
outer-range correction, not an inner deadzone. Small pushes remain analog; all
angles use the same continuous radial curve. Direction, right-stick turning,
buttons and triggers are retained. No automatic Pico profile detection is used.
Restart after INI edits. Wonderlands additionally provides live VR Settings ->
Stick full speed at (100/95/90/85/80/70/60%). Other two mods currently use INI only.

```ini
[Diagnostics]
MovementStickProbe=1
```

Optional evidence logs raw and calibrated vectors and cumulative forward/back/
left/right peaks. Maximum 60 reports, five seconds apart, per process. Move to
all four stick edges while stationary in a safe scene. Disable after testing.
Wonderlands samples only active gameplay; Singularity calibrates the logical
movement stick after SwapSticks. The profile remains vendor independent.

## Offline evidence

Identical movement_stick_range.h and test corpus are kept in all three projects.
The optimized tests retain assertions and cover 360 angles x 101 magnitudes at
four range settings: monotonic speed, direction preservation, unchanged default,
full speed at the configured boundary, zero/invalid samples, parameter bounds
and diagnostic report limits. Boundary assertions use a floating-point tolerance.
A 0xc0000409 observed during the first test run was this test's failed assertion
at a rounded floating-point boundary, not a game/runtime crash.

Wonderlands x64 Release + NGX build PASS; seven selected CPU/GDI CTests PASS,
including settings persistence/layout and pad mapping. Darkness II x86 production
DiagnosticCapture build PASS and standalone /O2 /W4 /WX test PASS. Singularity x86
/O2 production build PASS and standalone /O2 /W4 /WX test PASS; the existing
production static-analysis/deprecation warnings remain. No game/GPU test was run.

## Runtime gates

Use the accepted profile/DLL backup and a fresh shared-lab lease. First verify
outer=0 versus baseline. Then compare 10 and, only if needed, 20 on the affected
Pico: north/east/south/west, diagonals, gentle walking, sprint, head turns,
head-relative movement, menus and any stick swap. Confirm full speed from raw
axis values and actual displacement, not settings UI alone. The simulator can
prove numerical forwarding but cannot reproduce that physical controller.

The VDXR flatscreen-startup report in Darkness II/Singularity is a separate,
unresolved issue awaiting player logs. Calibration does not change OpenXR instance,
session creation, loaders, runtime registry or startup packages. Full body /
wrists-only / weapons-only presentation is also a separate feature, not included
in this input candidate. No FPS improvement is claimed.
