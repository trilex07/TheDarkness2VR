# Next bounded candidate: complete Present result accounting

Prepared offline on `codex/diagnostic-engine-present`; not deployed or launched.
DLL SHA-256: `495A41A7043ED4AADE438554574F6E1972BCC1420644F906EDFF4E9ADA5187C2`.

The sole runtime change is diagnostic Present result accounting. All returns
increment exactly one of ok/statuses/errors; every nonzero HRESULT logs
immediately, including between the 1000-call summaries. Negative HRESULTs latch
the existing dispatch fault, preventing later capture acceptance even after a
successful retry. Actual forwarding remains reported truthfully; dispatch
fault/unexpected caller explicitly reject capture before publication.
No FOV, draw, shader, controller, resolution or standard INI change.

Offline verification: x86 call-site/ABI/recursion/rollback test PASS including
an intermittent error at call17, positive occlusion status and failed-then-good
sequence. Seven readiness-checker tests and 15 matched-capture tests PASS.
Diagnostic DLL builds. The checker correctly refuses to promote the previous
47-row sampled log into complete HRESULT accounting; the previous 44,000-call
forwarding evidence remains valid within its documented limits.

`python tools/check_present_readiness.py <mono.log>` checks effective mono mode,
installation receipt, exact-caller identity, every recorded counter prefix,
nonzero statuses/errors, recursion, exception markers and counter regression.
PASS covers only the prefix through the latest counters, not unsampled shutdown
or loaded gameplay. It never authorizes VR automatically.

## Concrete next test awaiting separate authorization

The user's latest explicit approval covered only the completed mono trial of
7b5ee3d and prohibited automatic transition to VR. Therefore this candidate and
its one-frame VR capture have not been installed/run.

After separate approval, re-audit EXE and back up/hash current DLL/INIs. First
verify this candidate briefly with mono, OpenXR off, capture off and complete
return counters. Stop on any exception, recursion, error or unexpected status;
never silently retry. Only an explicitly authorized successful transition may
enable OpenXR, SubmitMode=4 and StereoMode=1 for the one-frame capture below.

Keep MatchedFrameCapture=0 until the user is in a loaded scene where the right
tentacles should be visible, preferably also containing a yellow peripheral
region and near/distant textures. Confirm effective mode/config from the log,
including any persisted lighting-preset overrides. Then request exactly once:

```ini
[Diagnostics]
EnginePresentCallsite=1
PresentReadiness=1
MatchedFrameCapture=1
MatchedCaptureSerial=1
```

Use a new serial only if necessary to differ from an already consumed serial;
do not automatically retry an incomplete capture. No native FOV modification.
Require COMPLETE.json, SHA checks, source-eye0/1.bmp, final-eye0/1.bmp and
consistent frame/device/thread/epoch, matrices, viewports, draw banks and poses.

Primary question is VR-001/P0: at which source/replay/final stage are right
tentacle pixels missing? Draw records support attribution but do not by
themselves prove visible pixels. Localize VR-002 yellow pixels at the same
stages. Use Luna ray-compare for VR-003 only after the checker accepts supported
source camera/FOV and pose correspondence. Do not invent camera-proof metadata
to force a numerical answer. Preserve a rejected/incomplete result as evidence.

Finish by exiting the test process and restoring/hash-checking all previous
DLL/INI files and unchanged EXE. No automatic parameter sweep or second VR run.
