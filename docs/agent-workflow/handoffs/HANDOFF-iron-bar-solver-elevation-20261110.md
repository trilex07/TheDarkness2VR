# Review packet B: measured elevation only after native solver failure

Date: 2026-11-10. SIDE darkness2. Branch: codex/iron-bar-solver-fallback-20261110.
Worktree: C:/Users/Trilex/Darkness2VR/.iron-bar-elevation-20261110.
Independent clean base2567d04072e9a38c66faec017e3b2779471935e5 / installed39D.
No retry-A, BFE cursor,088 trigger, turning or installed/profile changes included.
DefaultOFF: `[Input] ThrowSolverFailElevation=0`; no current INI was edited.
Status: minimal source/CPU review packet; **no product DLL built or activated**.
This does not establish that the player's fence throw entered this branch.

## Producer, units and coordinates

1. `d3d9.cpp:LocateTrackedHands` locates grip in `g_xrSpace`, at predicted display
   time. `XrSpaceVelocity.linearVelocity` supplies reference-space metres/sec;
   `vr_input.h:Motion::Update` otherwise differentiates located positions/time.
   Recovery/gap/jump gates apply to both. `ThrowStroke` selects the coherent
   measured velocity; `out.throwVelocity` passes it to `PublishNativeThrow`.
2. `ThrowDirection` takes velocity **by value** and writes a separate unit
   direction. It does not overwrite the publisher's measured velocity. The
   candidate records `velocity.y/Length(velocity)` from that same original
   argument before it is lost to the full aim contribution: a dimensionless
   sine of measured elevation, not a native speed or invented loft.
3. `UpdateHeadGeometry`/recenter construct `g_headGeometryReference.orientation`
   from **yaw only**, explicitly preserving gravity. XR Y and that reference Y
   therefore agree. Z is reflected to LH in the existing publisher; Y is not.
   Native world +Y is the established engine world-up convention documented in
   `docs/research/darkness2-camera-shake-20261101.md` (level world +Y parent-camera
   stabilization) and used by the existing camera math.
4. The existing `StabilizedCameraWorld` chooses current camera+80 or cached
   correction*current; `BuildRayMatrix` composes token direction with its rows.
   Camera pitch/roll can change the resulting world-Y component. The candidate
   consequently does **not** insert elevation in camera-local direction before
   that multiplication. It first gets the actual composed **world ray**, keeps
   that ray's XZ azimuth, then sets worldY to measured sin(elevation) and scales
   the horizontal unit component by sqrt(1-Y*Y). This preserves measured elevation
   despite camera tilt. A degenerate/invalid azimuth leaves the old fallback.
5. The native consumer still measures the existing velocity length in native
   units, multiplies only the chosen unit direction by that same length, and
   leaves W unchanged. No metres-to-native speed conversion occurs. Selector
   origin, actual native release point, magnitude and successful solver result
   are untouched. No new native call, hook target or collision bypass is added.

## Strict branch gate

`targetApplied=false` alone is insufficient: it also means no solver call.
The new token flag `targetSolverFailed` starts false. The **existing** callback
observing native E0BCA0's returned AL sets it only on the owned render-thread,
unexpired token with an already selected native target and AL==0. AL!=0 records
success and clears failure. The ABI-preserving trampoline remains byte-for-byte
unchanged. The fallback requires opt-in + explicit failure + !targetApplied +
valid measured speed (.8..15m/s, existing powered-stroke range). Thus no selected
target, no solver call, selected-but-not-called and native success stay unchanged.
No target query/range/cone/filter/priority or token lifetime changes.

## CPU proof and limits

```powershell
$env:TEMP='C:/Users/Trilex/.codex/tmp/darkness-solver-elevation-20261110'
./test/build_protected_left_throw.ps1
```

Private receipts: `captures/solver-elevation/red.log` and
`green-coordinates.log`. RED disables only the new fallback application and
fails the actual consumer's positive-Y expectation. GREEN: **475 checks PASS**
plus21 pinned native instruction contracts. Existing native ABI/character/
ownership/one-shot/speed/W/selector tests remain passing. Added controls cover:

- OFF, no selection, selected but no solver call, AL success, selected AL failure;
- exact located motion before blending `(1,2,-2)m/s`, elevation2/3;
- actual extracted **StabilizedCameraWorld**, actual production matrix Multiply
  and BuildRayMatrix, and actual TryNativeThrowVelocity with pitch30/roll45;
- stabilization OFF, current cached yaw correction and stale-cache native
  fallback: composed camera elevation differs, but result worldY remains10/3
  at native speed5; composed XZ azimuth, W=.625 and native camera bytes stay intact;
- negative/zero/positive measured elevation (including actual consumer), vertical/invalid aim azimuth,
  invalid speed0/16/Inf/NaN, corrupt elevationNaN, native W/magnitude, expired or
  mismatched ownership refusal with the feature enabled.

`EnsureCameraStabilizerFrame` is inert in this fixture with an explicitly
prepublished CPU snapshot; no engine/GPU update is executed. This tests actual
consumer composition and fallback paths, not live cache production or headset
gravity calibration. The fixture gained the required frame-id field for the
actual stabilizer body. An initial test expectation that all corrected rays
pointed downward was corrected: a valid cached yaw correction changes azimuth
and can also change that pre-fallback Y. The test now proves the correct invariant
of unchanged measured world elevation through every branch.

No product build/freeze/install or runtime trial yet. Review only B's source
diff from2567d040 before selecting a future one-behavior trial. A76/BFE/shared
simulator/installer and current39D are untouched. Rollback remains fresh current
39D/profile/saves/config under a separately granted serialized session; no
historical saves and no combined A+B candidate is proposed.
