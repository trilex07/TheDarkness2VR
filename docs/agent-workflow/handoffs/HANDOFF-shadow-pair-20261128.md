# Same-frame shadow measurement

- **Date** 2026-11-28
- **Task ID** shadow-pair-20261128
- **Role** MAIN
- **Branch** `codex/darkness2-shadow-pair-20261128`
- **Worktree** `C:/Users/Trilex/Darkness2VR-shadow-pair-20261128`
- **Base** `5f3c407b`, main simulator fixes
- **Integration** Pending a successful in-game capture

The crane light changes its native transform between consecutive captures.
All eight matched live and replay draw pairs in retained frames 8195, 8286,
8378 and 8474 had identical native light constants at c0, c1 and c16–19.
The eye transform at c29–30 differed by up to 0.684. The before and after
images of the first shadow draw had identical SHA-256 values in all four
frames. Use the second draw first in this scene.

The diagnostic build accepts `CapturePassStages=5` and the zero-based
`CaptureDrawOrdinal` in `Darkness2VR-diagnostic.ini`. For the selected
`C4D1FB10` draw it saves the input lighting target, an R32F copy of its
actual INTZ depth input and the resulting lighting target in each eye.
The paired capture cap is 512 MiB so the six images fit at 3345×3410.
The validator requires the same draw ordinal and a separate producer,
depth copy and consumer viewport for each eye. The audit uses the actual
blend state and RGB target values to recover the multiplicative shadow
contribution. It compares depth-consistent reprojected pixels after
verifying equal native light constants.

## Evidence

- `build.ps1 -DiagnosticCapture` passed. The DLL SHA-256 is
  `2B9842C7650DF1680A83C3DFABCC35F70F177863F5C37ACC88718A777D49345B`.
- All 47 matched-capture tests passed. The paired synthetic audit recovered
  a 16-pixel eye shift with zero mask error and refused a `1e-8` light
  constant difference. The both-eye shadow scope fixture passed.
- All four earlier real mode-3 captures passed validation. The temporal
  audit refused two of these captures because their light projections
  differed. A legacy capture without paired stages was refused in pair mode.

## Remaining gameplay checks

1. Use the guarded isolated chapter launch and establish the Villa gameplay
   view. Start with ordinal 1 for the crane. Select the observed consuming
   ordinal for the Villa.
2. Run `python -B tools/audit_shadow_temporal.py <capture> --same-frame`.
   Compare the reported region in both final eyes. Retain the full paired
   files if they are required to reproduce the diagnosis.
3. If the audit rejects the capture, retain the rejection, native draw
   identity, configuration and hashes. Resolve the failure before changing
   light or shadow shaders.

The shared lab remained reserved. No chapter, ladder or paired shadow
capture ran from this worktree.
