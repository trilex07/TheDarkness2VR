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
