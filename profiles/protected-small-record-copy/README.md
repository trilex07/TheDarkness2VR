# Isolated recorder-copy candidate

Implementation: `src/small_record_copy.h`, called by actual `RecPush` in
`src/stereo_replay.inl`; integrated as804d0e7, actual-body regression asaecf54a.
OFF by default. IMPLEMENTED / OFFLINE VERIFIED; game effect NOT VERIFIED.

These are overlays, not replacements for the game's complete INI. MAIN applies
one AFTER the approved protected-controls-head-gestures configuration. Compare
`off.overrides.ini` against `on.overrides.ini` with the SAME DLL, resolution,
scene, head/input/gestures and diagnostic INI. The sole difference must be
Render/SmallRecordCopy0/1. AsyncLog, sampler caps and reader-fence experiments
are explicitly OFF in BOTH arms. Root/default INI remains untouched.

The opt-in replaces fresh VQ+copy only for1..192-byte buffers with exact-count
self-RPM. Any failure/partial copy runs the unchanged fresh-query legacy
prefix/zero-tail fallback. Guard semantics must pass on owned allocation before
device hooks install. No source-page permission cache. No command, resource,
fence, FOV, source-resolution, native EXE or eye-generation change.

Before accepting B, require the actual effective startup receipt
`[SmallRecordCopy] requested=1 admitted=1 guardProbe=1`. A rejected probe is
NO_GO, not a successful comparison. This is not a promise of90Hz: current
retained combat RecPush costs1.63/1.99ms INCLUSIVE; only part is removable.
Historical payload coverage97.76% is not current outdoor time coverage.

Run the existing actual-body small-record-copy test and profile composition
regression. MAIN alone owns deployment after a new authorized session and fresh
EXE/backup/free-session checks. No automatic launch. Preserve source DLL/PDB
identity and existing installed backup. Rollback: select the OFF overlay and
restart, or restore the saved DLL/all INIs. See the matched A/B receipt in
`docs/research/async-log-ab-outdoor-20261026.md`.
