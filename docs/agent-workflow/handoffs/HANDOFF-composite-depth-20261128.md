# Paired composite depth capture

- **Date** 2026-11-28
- **Task ID** composite-depth-20261128
- **Role** MAIN
- **Branch** `codex/darkness2-composite-depth-20261128`
- **Base** `3201fbd1`, paired shadow diagnostic
- **Integration** Pending gameplay validation

The composite fog-height study uses verified shader instructions and recorded
device banks. It needs samples from the actual composite depth texture to
check the numeric depth convention and evaluate the reported lighting.

In the diagnostic build, `CapturePassStages=6` saves both eyes' sampler 2
inputs from the first actual `33DE98C1` draw. The capture copies retained INTZ
depth to R32F before each consuming draw. It records the original sampler
state, native depth attachment and both shader identities. It requires exact
restoration of both constant banks and a successful draw return in each eye.
The existing mode 5 still captures the paired shadow pass.

## Validation

- `build.ps1 -DiagnosticCapture` passed. DLL SHA-256
  `A7C28563FD1143920A55B8500C83EDD69199734E9772B314E12EF106722B890B`.
- All 101 top-level Python tests passed. The new cases require both matching
  INTZ producers, valid sampler queries and successful draw returns.
- Four retained real mode-3 captures still validate. The chapter profile
  preflight passed. Offline navigation checks accepted modes 5 and 6 on
  this DLL and rejected mode 6 on the earlier paired-shadow DLL.

## Gameplay

Use the isolated chapter fixture and `capture_composite_depth_pair(serial)`
from its private navigation helper. Validate the receipt, inspect the
producer viewport and compare sampled depth against the observed projection
before changing the fog shader. Preserve the scene in both eyes and capture
paired shadows separately when the affected shadow is visible. The lab
reservation still prevented a gameplay run of this candidate.
