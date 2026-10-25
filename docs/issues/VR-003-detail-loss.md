# VR-003 — Insufficient detail/sharpness

Status: OPEN, loss stage UNKNOWN. Work after P0 visibility localization.
Source: user's previous PSVR2 test, not the new mono trial.

Symptom: image clarity remains mediocre. No assumption that render resolution,
depth of field, mip selection, resampling or the headset compositor is responsible.

Next evidence: lossless matched D3D9-source and final-D3D11 images from both
eyes, with known camera ray mapping, viewport/crop/filtering and output sizes.
Run the capture checker before Luna's ray-compare. Unsupported camera/pose
mapping blocks a numerical sharpness conclusion; do not substitute an inferred
GetFOV value or unrelated screenshots. Compare textured near/distant regions
within the supported common ray domain to locate actual detail loss.

Acceptance: measured improvement at the established loss stage without new
geometry/lighting/stereo regressions, followed by user headset feedback.
No blur fix or sharpen/resolution change has been made.

2026-10-25 evidence: actual source RT0 is 1280x855 despite preserved INI request
3076x3140 in the diagnostic variant. This is a source-resolution limitation for
this run, not proof of all earlier headset blur. Missing final eyes and valid
paired camera evidence block Luna comparison. See ../research/staged-capture-20261025.md.

2026-10-25: complete frame5408 proves a separate false-motion-vector defect in
exact MotionBlur_16 VS73EBA724: transformed current c0 versus native previous c4.
Opt-in FixMotionVectorProjection corrects the previous matrix for this reflected
shader only, excluding head geometry/history. Offline captured-matrix error drops
below1.8e-8 for a stationary point; native motion and stereo disparity preserved.
Not deployed/tested in PSVR2. Resolution/sharpening unchanged. Luna remains NOT
RUN: current capture finals carry the same right-eye source (transfer defect fixed
offline), and player-camera ownership is not established. No overall blur fix claimed.
See ../research/vr001-render-fixes-20261025.md.
