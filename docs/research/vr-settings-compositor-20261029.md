# VR Settings compositor panel

This branch inherits `99cefa4` (VS and PS eye carry, DirectRecordCopy diagnostic
switch, and wrist palette correction). It does not replace the installed Steam
DLL. Main/pause/options integration and runtime headset acceptance remain open.

## Implemented

INSERT opens/closes a world-anchored 1.28 x 0.72 metre panel, 1.5 metres in front
of the current head. `[Input] VRSettingsKey` accepts a virtual-key binding;
45 is INSERT and 0 disables it. Existing F1-F12 mod shortcuts cannot be assigned.
The right aim pose intersects the exact OpenXR quad pose/size/640x360 image rect.
Hover, trigger selection and B/Close work on this surface. A held trigger must be
released after opening, tracking recovery or pending-surface recovery.
This input path uses the existing enabled OpenXR gamepad bridge.

OFF/AUTO/HALF change the existing adaptive request. AUTO/HALF explicitly display
UNSUPPORTED: this build has no supported session-scoped runtime half-rate adapter.
No display-refresh request, sleep, frame skipping or synthetic reprojection is
used as a substitute. Requests currently persist only for this process.

XR controls are consumed before triangle, recenter and gameplay gestures while
the panel owns input. Closing retains this ownership until trigger, buttons,
sticks and grips are neutral. Physical XInput/keyboard paths are not modified.
Repeated polls within one XR frame retain hover without repeating a click.

The renderer uses a separate negotiated BGRA swapchain and CPU GDI text. It caches
the raster by mode/cursor/binding and uploads only changed revisions to acquired
images. It uses no game texture, native menu coordinates or eye replay state.
There is one extra compositor layer only while the panel is successfully drawn.

Acquired images remain owned across zero-timeout waits. Failed releases retain
the waited image for retry; invalid images or non-success receipts cannot
publish a clickable panel. Focus loss closes it; reference-space change closes
it; instance loss closes it; shouldRender=false, absent world image or image timeout suspends it. Session
STOPPING releases the panel resources before xrEndSession. No XR calls are made
from a global destructor/DllMain. Failed swapchain destruction retains ownership.

The existing world frame still calls xrEndFrame exactly once. No added
xrWaitFrame/xrBeginFrame. Adaptive performance accounting receives the world
layer count, not the settings layer. Matched eye captures exclude overlay frames.

OpenXR swapchain ownership was compared with Khronos hello_xr (Apache-2.0),
`src/tests/hello_xr/openxr_program.cpp`, in the official OpenXR-SDK-Source project.
No implementation text from that project was copied.

## Offline validation

`test/build_vr_settings.ps1` compiles actual headers/renderer and extracts the
actual host input/submission blocks. It runs without a GPU, XR runtime, Steam or
headset; GDI draws only a private CPU DIB. Fixtures cover:

- Panel geometry and yaw, fresh trigger release, Back, invalid space/tracking,
  duplicate frame ownership, neutral-input lease and mode selection: 2068 checks.
- Real GDI output and deletion receipts; stable GDI object count after repeated
  allocation/release. Windows keeps one process GDI object after first text draw;
  subsequent tests compare the warmed baseline (1) with final count (1).
- Actual renderer API failure paths, layer capability, format/image admission,
  acquire/wait/release ordering, pending timeout, release retry, index bounds,
  per-image revision cache and failed destruction.
- Actual input route: pending/open/closed ownership, held L3+R3, held trigger,
  successful-neutral recovery, focus and disabled binding: 50 checks.
- Actual submission block: all combinations of panel open, focus, shouldRender,
  world layer count, panel receipt and xrEndFrame receipt. 1803 checks, exactly
  one xrEndFrame and unchanged adaptive world accounting.
- Actual PumpXR recovery with a fake runtime/event queue and external COM Present
  fixture: closes the panel on focus/space/instance loss, requires key release on
  reacquisition, and destroys panel resources before xrEndSession. Existing
  refresh/adaptive leaf dependencies were added to the older test scaffold;
  their own policy/API behavior is covered by separate fixtures.

Both x86 DLL builds pass. Existing actual replay float loop passes 58510 checks
in each flavor; XInput bridge (97 production/151 diagnostic), left aim (461),
6DOF pointer core (181360), and refresh adapter (360) pass.
Adaptive policy previously passed 3243 checks; its host integration binary could
not be launched because of a persistent OS sharing lock. No rename/bypass used.

The fixture writes `vr-settings-preview.bmp` for CPU visual review. This is not
evidence of correct compositor placement or readable size in PSVR2.

## Remaining

This is the VR Settings surface only. The follow-up laser branch adds a thin
3D beam from the tracked aim origin to the existing hit marker. Native main/pause/options still require a proven native menu state,
real submitted surface and cursor/input bridge; no guessed native plane or
SendInput imitation has been introduced. Main-menu sliders and scrolling have
not been integrated. In-headset input/visual correctness and FPS impact are
unmeasured. Keep the accepted Steam performance baseline until consolidated
authorized deployment/PSVR2 validation with the right-eye candidate.

## Tracked beam follow-up (offline)

`codex/vr-settings-laser` reuses the current released settings swapchain image:
a 1x1 sub-rectangle at the cyan hit marker supplies the beam colour. No new
swapchain, upload, image acquisition, fence or XR frame call is introduced.
OpenXR explicitly allows multiple layer references to the same released image
with different rectangles: https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html

The beam is a 2mm quad, trimmed 20mm from the aim origin and 2mm before the hit.
Its long axis follows the actual controller ray; its front faces the current
head. Degenerate collinear views retain only the hit marker. Invalid tracking,
space, pose, stale frame, pending image ownership, or runtime layer caps below
three omit the beam. Painter order is world, beam, panel. All layers use the
same existing xrEndFrame; adaptive accounting still counts only world layers.

Both x86 builds pass. Actual-source CPU regressions: geometry/input 4518 checks,
renderer 863 (including cyan texel, shared released image, caps and ownership),
host input 50, submission 7215 (beam enabled/disabled, view availability, panel
status, XR result and layer order). Real GDI objects return to the warm baseline.
Headset placement, per-eye appearance and runtime cost remain NOT RUN.

No follow-up deployment: after the user's crash the deployed 8841def candidate
was backed up and rolled back to af9cdd7, DirectRecordCopy OFF. The Polish pack
was separately backed up and its twelve archive/TOC files restored from SIDE's
original backups after finding unsafe numeric header rewriting in its injector.
The specific crash cause is not established. Do not silently re-enable DirectCopy
or reinstall that pack as part of this beam work.
