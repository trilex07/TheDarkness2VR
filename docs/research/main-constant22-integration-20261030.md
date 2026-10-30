# MAIN exact constant producer extension

Integrated the user's uncommitted SIDE candidate from codex/render-budget-90hz
(render-cpu-optimization worktree, base b7ca547f). Only native_constant_source.h,
the three test additions and the private-image auditor were transferred.
MAIN StateBlock includes and successful/failed PS history tests were retained.
No source worktree files were modified.

Adds twelve exact 16-byte global submission sites and ten current-owner scratch
submission sites to the existing synchronous, SHA/fingerprint-admitted copy
contract. Exact API/parent/call site, duplicated arguments, size and current
source equality remain required. No cached values, permission cache, new locks
or GPU synchronization. Existing NativeOwnedConstantCopy and
NativeOwnedMaterialCopy switches control the extension. No DirectRecordCopy
change; ReplayBufferVersions remains quarantined.

Both full x86 builds PASS. Actual-source native constant suite: 412337 checks
PASS, including MAIN StateBlock coverage and fresh scratch/global revisions.
Private installed-image audit: twelve global and ten helper paths PASS.
StateBlock, live texture/depth, VB/IB consistency and bounded reentrant callback
probes, replay binding cache and projection history PASS. Synthetic global
copy comparison: RPM 568.9 ns vs classified direct copy 16.1 ns; runtime
frequency and FPS gain are unknown. Headset/street combat acceptance NOT RUN.

SIDE reports an earlier automatic publication rejection for proprietary-derived
identification metadata. This integration includes numeric source/callsite
addresses and fingerprints; no extracted code bytes, assets or private logs.
Publishing must pass the applicable review; local build/test is separate.
