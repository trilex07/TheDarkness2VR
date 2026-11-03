# Reflection eye-coordinate contract

Private matched captures 50820/901/32722, 50820/902/35153,
9284/941/19617 and 9284/942/22654 admit exact VS12CCC8DB, 321 words,
SHA256223298007ec4e1942d8e4ec600c508b9e677a2b3afa8f2090047579ea67bcca3.
Its c0..3 WVP writes clip position. c8.xyz minus input position feeds three
tangent-space DP3s. c8.w is animation time and must be preserved.

The shader's cube-reflection PS7DCA4CC4 is 1596 bytes,
SHA256df4b3b0c641a4bb21f4445d5193927d201a7f0e9aa0d9c42ea363299ea2911e9.
Its normalized tangent-space view vector is reflected about tangent Z,
transformed by tangent basis, and used to sample cube sampler4. This is not a
screen-space texture-coordinate lookup.

## Measured mismatch

For row-vector perspective WVP M, the model-space camera center is the
homogeneous vector (0,0,1,0)*inverse(M), divided by w. Undoing the captured
mode0 stereo transform (eye translation, head clip, FOV scale in reverse order)
recovers the native center. Existing c8 agrees with that native center:
maximum errors across the four captures are 0.000414, 0.000414, 0.000388 and
0.000312 game units. The center of the actual rendered WVP differs by median
0.0325 game units in each eye, consistent with half the configured baseline.
There are 25,13,57,159 consuming draws per eye respectively.

This supersedes the overly broad inference that identical c8 under pure head
rotation exonerates this path: identical native c8 is expected for a fixed
native camera, but does not account for the VR eye baseline. It does NOT prove
this shader produces the user's drifting floor pixels, nor that 0.0325-unit
specular disparity explains the entire visible artifact. The late selected
camera VP cannot be substituted for each object's original camera ownership;
it disagrees on some frames. Use the actual stereo transform and raw WVP.

Read-only reproduction: tools/audit_reflection_eye.py <matched-folder> [...].
Strict capture validator, full vertex-bytecode SHA, two-eye coverage, supported
policy and native eye/geometry residual gate are required. All four captures
PASS. No native shader bytes or images are committed.

## Bounded experimental implementation

Diagnostics/ReflectionEyeExperiment is startup-only and OFF by default. Exact
complete SHA-pinned VS AND PS are required, alongside geometric-head same-frame
stereo. No generic c8 rewrite. At each matching draw:

- require valid raw native WVP/c8 from the current live/replay DEVICE bank;
- query the actual device WVP/c8, reject device/raw c8 disagreement;
- verify native c8 matches raw-WVP camera center within0.001 game units;
- replace c8.xyz with actual rendered-WVP center, keep time w unchanged;
- restore the exact device c8 after the draw, including failed-set attempts.

This handles inherited/partial uploads without using shader-local stale state.
After stateblock history invalidation, unknown ownership rejects the operation.
Additional query/set calls are an explicit experiment cost, not an optimization.
The default-off path performs none of these driver calls. No shader bytes,
geometry, resolution, shadow parameters or unrelated constant registers change.

Actual production-scope fixture PASS: both eyes, transformed/scaled model,
ownership rejection, unknown raw state, read failure, setter failure and exact
restoration. Diagnostic build PASS, video observer fixture PASS, capture42 PASS.
Runtime experiment and visual acceptance still pending at this checkpoint.
The frozen combined-scenes-final package is unchanged and does not include
this new experimental code yet. No performance or floor-shadow fix claimed.
