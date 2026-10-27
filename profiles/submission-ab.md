# Grouped replay submission candidate

Optional Render.TriangleHoldRecenter=1: on the OpenXR left triangle/Y action,
hold continuously for one second to recenter once. A short tap sends the
existing START/menu action on release. Long release sends no menu action.
Tracking/focus discontinuity or polling gap cancels the gesture and requires
release before rearming. Separate menu action, F8 and submission L3+R3 remain.
This option defaults OFF; it does not change physical-pad button mappings.

Overlay Render keys: ReplayConstantBatch=1, ReplayConstantWindow=1,
SubmissionPadToggle=1, ObjectDrawPadToggle=0, SkipObjectDraws=0,
SkipShadowAtlasDraws=0. Preserve all other CPU/stereo/input settings.

Starts with grouped constant submission enabled. L3+R3 alternates grouped ON/OFF
at the VR-frame boundary, preserving visible geometry. Release both sticks
before each click. F8 still recenters; the chord temporarily replaces recenter.
SubmissionPadToggle takes priority over and disables geometry/shadow omission.
Clicks do not persist settings; restart restores configured initial state.

This enables the existing byte-preserving replay constant-window assembler,
not a new geometry renderer or GPU-driven instancing. Each original upload is
transformed before aggregation. VS/PS banks remain separate. Every non-float
command flushes pending writes; no draw, resource change or state observation
is crossed. There is no persistent state cache or native/EXE modification.
Live-eye native submission, draw count and geometry remain unchanged.

Current pre-candidate crowded-window receipt had 274036 VS plus 205758 PS
uploads /120 replay streams, approximately3998 calls/stream. This is an
opportunity count, not a saving estimate. `[ReplayUploads]` reports actual
driver call entries and failures; `[SubmissionAB]` records switches. Discard
120-stream windows spanning a toggle. Compare the same position and heading
for at least five seconds per mode. Report both FPS/CPU and visual differences.
The fixed 90Hz display ceiling can hide headroom; do not infer savings from
call reductions alone. This candidate may not address the dominant draw cost.

Offline actual replay loop: 72 streams /432072 records /62893 barrier snapshots
in each x86 mode; bit equality, stage separation, caps, failure and exception
behavior PASS. Input bridge chord/release tests and both DLL builds PASS.
Actual game image and FPS acceptance remain pending.

Rollback: deployment backup, or ReplayConstantBatch=0,
ReplayConstantWindow=0, SubmissionPadToggle=0 then restart.
