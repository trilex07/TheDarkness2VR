# Grouped replay submission candidate

## Combined CPU submission package

Set SubmissionPackage=1, WideRecordCopy=1, UniqueReplayResourceRefs=1 along
with the grouped ON overlay below. L3+R3 then alternates ALL THREE additions:
constant grouping, wide copy and unique frame references. Existing accepted
SmallRecordCopy/SamplerCapsCache/AsyncLog/reader fence/transfer worker remain
unchanged in either mode. TriangleHoldRecenter stays available.

Wide copy extends admitted exact-count self-ReadProcessMemory from 192 bytes
to at most 64 KiB, covering larger matrix/bone packets. It still needs the
existing guard admission and uses the unchanged readable-prefix/zero fallback
on failure/short count. There is no source-page cache, delayed native pointer
or read beyond the requested payload. Above 64 KiB uses the existing path.
This exchanges validation+copy for one kernel-mediated copy; runtime benefit
depends on packet sizes and must be measured, not inferred from syscall count.

Unique refs uses the existing exact-interface-pointer per-frame lifetime ledger.
It owns one reference through replay, not one per repeated bind; interface
aliases stay distinct. Frame Begin locks the selected ledger mode, so a toggle
cannot mix retain/release policies inside that ledger. `[ReplayRefs]` reports
requests versus retained references. This does not deduplicate native objects
or cache resources across frames. Hash-set bookkeeping may offset savings.

Offline: actual x86 recorder and wide direct/failed/short/guard/noaccess reads,
193..65537-byte boundary cases and arena offsets PASS (549 byte/range checks).
Frame resource tests PASS across 10000 frames/mode, owner drops, both eyes,
reset/abort and mode changes. Synthetic COM pair reduction is not game FPS proof.
This remains a CPU command-path candidate, not GPU-driven geometry rendering.
Rollback all additions with WideRecordCopy=0, UniqueReplayResourceRefs=0,
ReplayConstantBatch=0, ReplayConstantWindow=0, SubmissionPackage=0,
SubmissionPadToggle=0 and restart, or restore the deployment backup.

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
