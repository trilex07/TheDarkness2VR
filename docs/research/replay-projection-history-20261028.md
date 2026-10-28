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
