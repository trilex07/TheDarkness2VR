# Ordered projection history for right-eye replay

## Reproduced defect and correction

The actual production `ApplyShiftToUpload` failed a deterministic identical-eye
test before this correction. A legal sequence is: the previous WVP is A; the
first live command uploads a separate STP(A); a later object uploads WVP B.
Replay began with the shared shader metadata already holding B. Its first
STP(A) therefore failed the WVP/STP identity check and received a different
transform, despite identical source data and eye parameters.

`projection_history.h` now maintains separate live, frame-start and replay raw
WVP histories. Each used shader snapshots its history lazily before its first
live command in the recording generation. Replay starts from that snapshot and
updates only replay history. It cannot overwrite the next live frame's history.
There are no extra device calls, source-memory queries, record payloads or
per-upload allocations. Reset/ResetEx invalidate histories.

A separate existing identity defect also required correction: new vertex
shaders at an already seen COM address previously kept all old analysis.
Full bytecode comparison now distinguishes immutable analysis reuse from an
address recycled for different code, even when its 32-bit hash is equal.
Only an exact owned-interface/bytecode match skips repeated analysis; the
original native shader creation still always runs synchronously.

## Validation and limits

- Before correction: actual-source identical-eye mismatch reproduced.
- After correction: `test/build_projection_history.ps1`, 2,445 checks PASS:
  split STP/WVP ordering, future live object isolation, next-frame inheritance,
  distinct shaders, exact bytecode/address reuse and Reset invalidation.
- Actual tentacle scope: 19,062 checks PASS across 1,890 scopes.
- Actual replay surface status: 96 streams / 576 calls PASS.
- Production and diagnostic x86 builds PASS.

These are CPU/source regressions, not a new real-game or HMD run. The defect
is a proven source of different replay output; occurrence/frequency in the
reported right-eye flicker remains unmeasured. Dynamic buffer content versions,
partial inherited constant banks and other outstanding stereo causes are not
claimed resolved. No 90 FPS or permanent visual acceptance claim is made.

MAIN integration: use the diagnostic DLL built from this branch together with
`b17208d` (selected local player and independent arms). Keep the current native
2676x2728 profile. No INI changes are required. MAIN alone deploys with backup
and rollback while the game is closed; this work does not launch the game.

## Shared register correction, 2026-10-29

The per-shader history above was insufficient: binding shader A does not
restore the values A last uploaded. The actual-source regression writes WVP A
to c0 through c3, binds B and writes WVP B to those same registers, then rebinds
A and uploads STP(B) separately. The previous implementation fails check 13
(`memcmp(actual, expected)`); it classifies using stale WVP A.

Production now obtains its reference from a shared raw VS register bank,
updated from already owned recorder bytes under the existing recorder lock.
The frame-start snapshot and each replay start have independent banks. All
recorded float writes contribute, including other shaders and skipped passes.
Partial writes update only their rows; all four rows must be known before a
matrix is used. Unrecorded writes, recording overflow, failed native uploads,
Reset and externally applied state blocks invalidate affected knowledge.
Rejected native uploads also prevent publishing a complete replay eye.

There are no new device queries, native-pointer reads, per-upload allocations
or extra locks on the normal recorded upload path. This adds a 4 KiB raw-bank
snapshot at recording/replay start and copies recorded float rows into the
corresponding bank. No performance saving is claimed.

Validation: actual-source projection history 24,591 checks PASS; recorder
6,103 checks PASS including overflow invalidation; pixel shader metadata
100,000 concurrent publications PASS; replay packet equivalence 80,190 checks
over 360,000 operations PASS; projection estimator 100 transitions PASS.
Production and diagnostic x86 builds PASS. Other candidate features retain
their earlier validation; these numbers are CPU regressions, not HMD evidence.

Unknown inherited registers deliberately have no per-shader fallback. This
corrects the STP reference, not stereo injection for arbitrary partial WVP
uploads or dynamic VB/IB content versioning. It does not establish this defect
as the user's side disappearance or flicker cause. CULL OFF 5 in menus and ON
in gameplay is acknowledged; culling admission remains unchanged. MAIN should
use the cumulative candidate with its existing native-resolution profile and
backup/rollback, then verify both eyes and side visibility in one headset run.

## UI versus world activity correction, 2026-10-29

The live caller treated every true ApplyShiftToUpload result as a 3D-camera
upload, including orthographic sprite scaling. Draw wrappers then certified
world regeneration from this flag and shader c0 identity without checking the
current matrix. An identified UI shader could thus suppress right-background
restoration while drawing only an overlay. Recording restarts could also retain
the old world flag if SubmitXR had not reached its tail.

A separate perspective-WVP witness now excludes UI and STP-only writes.
Recorded draws check the current shared raw WVP under the existing recorder
lock: all four rows must be known and its perspective column finite/nonzero.
The world flag resets at recording start and latches on the first qualifying
draw. No new lock, external read, driver query or unconditional copy is added.
Diagnostic hidden-object/shadow modes conservatively retain history restoration.

PASS: 32616 actual-source projection/activity checks, including 1000 UI/world
transitions; right-eye history1000 world/UI plus1000 rejected segments;
recorder6103; UP geometry4344; production and diagnostic x86 builds. Recorder/UP
fixtures stub only the activity notification; projection tests execute its
actual implementation. These are CPU regressions, not physical headset proof.

Five retained complete captures passed hash/Present validation (104/122/145/154/
242 paired draws). They do not prove dynamic-buffer overwrites and predate
this candidate. No broad buffer snapshotting was added. Permanent flicker
resolution, partial redraw history, dynamic GPU contents, partial/inherited
stereo injection and native 90 FPS remain unverified or incomplete.

## Partial WVP upload completion, 2026-10-29

Actual ApplyShiftToUpload required a single upload to contain all four WVP
rows. A write to only c3 therefore overwrote a previously adjusted device row
with native data. The new regression calls this old function on a positively
identified, completely known matrix and fails check 3 (modified, line 80) with
`projection_history.exe --partial-legacy`. This proves a code defect, not its
frequency or contribution to the user's reported artifacts.

Live and replay now share ApplyVertexStereoUpload. When an original upload
overlaps part of an identified WVP, it completes the matrix from the current
raw device register reference, then runs the existing full transform. The
forwarded range expands to include those four rows only after transformation
succeeds. Surrounding uploaded payload stays intact; ranges remain contiguous.
The recorder retains the original native range/bytes and projection snapshot;
raw history never receives adjusted data. A native failure invalidates the
expanded range and recording; a replay failure remains incomplete, without
retry. The skin witness retains its original range and adjusts its forwarded
pointer to that origin.

Unknown rows, unidentified shaders, unrelated writes, full WVP uploads and
shadow/light-pass admission retain existing behavior. No getter, external read,
allocation, lock, persistent cache or new setting is added. At most three rows
are added to a 68-row original upload; destination capacity is 72 rows. The
completion scratch array exists only in a non-inlined partial-upload helper.

PASS: 34082 actual-source projection/activity checks, including both eyes,
head geometry, every partial WVP subrange, register-mask boundaries, unknown
history refusal, 68-to71-row expansions and destination guard bytes. Actual
replay production/diagnostic each passes 96 streams / 576096 records / 56834
checks, including a frame-inherited partial WVP with grouping OFF/adjacent/
window/binding-window, exact expanded API range, unchanged recorder bytes,
raw-register isolation and failed-driver completion rejection. Both x86 DLL
builds pass. No real-game/graphics-driver/HMD validation was run.

Partial STP/previous matrices and matrices inherited by a draw with no current
upload remain separate work. Dynamic VB/IB content versions and partial world
history restoration remain open. CULL admission is unchanged. Neither stable
90 FPS nor permanent disappearance of side clipping/right-eye flicker is proven.
