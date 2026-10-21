# Rendered FOV / OpenXR metadata correction

2026-10-21, separate from presentation-boundary commit b266bc5.

## Cause and change

The production live/replay transform widens a symmetric game projection. Its
D3D11 blit samples `uv = 0.5 + (outputUV - 0.5) * crop`. Nevertheless,
RenderEyePairFromReplay copied the headset's asymmetric FOV directly to the
projection layer. Thus identical center pixels were assigned different rays
in the two eyes. UpdateSubmissionFovFromMatrices computed symmetric coverage,
but that did not correct the final layer metadata.

For each axis the image's actual half-angle is:

    atan(crop / (projectionCoefficient * matrixScale))

The correction uses the observed camera coefficients and this pair's frozen
eye transform/crop. It changes projection metadata only, not IPD, convergence,
pixel shifts or calibration. Without a recognized game projection the existing
menu fallback remains. Asymmetric native game cameras need additional handling;
this correction is for the verified symmetric main camera.

## Evidence

- Gameplay capture PID 43052, frame_003 trace, command 2431: original PS c27..30
  inverse projection diagonal 0.5655826 / 0.5773503, hence Px=1.768088, Py=1.732051.
  These constants are embedded with provenance in rendered_fov_test.cpp.
- PID 9392, pair 3000: matrix scales 0.3070863 / 0.4344308, crop 1 / 1.
  Correct submitted FOV (left,right,up,down), radians:
  (-1.073377, 1.073377, 0.925725, -0.925725) in both eyes.
  Previously left received (-1.073377,0.758284,...) and right
  (-0.758284,1.073377,...), moving the center ray in opposite directions.
- Build/test: 1206 rays across captured projection, widen and crop combinations;
  invalid inputs and the actual old asymmetric center-ray mismatch tested.
  Run tools/stereo-math-tests/build-rendered-fov.ps1.
- Deployed DLL SHA256:
  9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C.
  Loaded barber level visually observed, over 3000 successful Presents before
  capture; both eyes rendered. Independent captures are not duplicates.
- User HMD result: **"Obraz składa się poprawnie"**, in response to the explicit
  stationary, looking-straight stereo check. This validates fusion in that test.

## Scope still unvalidated

HeadLookMode=0, HeadGeometry=0, StartupMode=2 in this diagnostic session.
Moving-head pose matching, metric world scale, native hands/aiming, and other
levels are not certified by stationary fusion. Native gameplay hooks were not
enabled. The historical default head-center submission pose is unchanged;
matching full 6DoF render cameras to per-eye layer poses remains separate work.
Usable independent PresentMon capture is still outstanding.

Local artifacts: test-output/rendered-fov-9392.log,
run_9392_frame_001_eye0/1.bmp and run_43052_frame_003_trace.txt.
