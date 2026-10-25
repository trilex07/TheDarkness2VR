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
