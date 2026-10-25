# Playable peripheral candidate — 2026-10-25

Based on `50a0d6f` / the headset-tested fog change `81dee71`. This candidate
restores the existing input hooks and attempts a native camera coverage fix.
It has **not** been deployed or tested on PSVR2. The user's last observation
was sharper graphics with missing peripheral geometry after fog removal.
That observation does not by itself prove CPU culling.

## Code change

`RestoreTrackedGameplay=1` in the diagnostic INI installs the existing aim,
throw, weapon, pistol and hands callbacks after the first CreateDevice, with
exact SHA/PE and 13 instruction/data pins checked before any writes. A partial
failure removes the bundle and checks the original bytes. This restores the
paths deliberately suppressed in the capture build. The game INI must also
enable HeadGeometry, NativeStereoHeadGeometry, NativeLeftAim, NativeWeaponPose,
NativeTrackedHands and TentacleGestureBridge. Keep HeadLookMode=0 to avoid a
second mouse-driven head rotation. Existing smooth turning, L3+R3, triangle
pause and grip actions are retained; GestureGamepadBridge enables the existing
objective/hip-reload gestures.

`NativeCameraCover=1` is a separate **experimental, OFF-by-default** switch.
It replaces only the five-byte CALL at `00986139`, in process memory. It never
changes DarknessII.exe on disk. This is a NEW native-camera patch, outside
the previously approved Present/fog trials; a live trial requires explicit
approval. The DLL is pinned by the prerequisite verified Present installer.
Install is only before first CreateDevice returns, never mid-frame or in DllMain.

The callback accepts only the render-selected `00FF2800` camera referenced by
the current owner's `+3F4` strong-reference slot, on the render thread before
3D uploads. It validates the complete native perspective matrix against the
camera's angle, aspect and clipping fields, then calls the original FOV setter.
The requested vertical angle conservatively covers both HMD lenses at the
native aspect; native aspect and clip distances are preserved. The engine
rebuilds P, VP, planes and bounds. Eye scaling uses the resulting P, including
contraction of surplus coverage, so GPU widening is not applied twice.
Pixel-space UI/scissor retains the original camera's angular scale.

The shim preserves GPRs, flags, x87 and SSE, and tail-calls the original near
getter exactly once. It retains no owning camera pointer after the call (the
diagnostic integer address is never dereferenced later). Initial transition
or mismatched camera/GPU projection invalidates capture. A stale selected-camera
frame suppresses the XR world layer. Neither production defaults nor the old
unverified GetFOV/FCOS hooks are enabled.

## New verified static path

Installed EXE SHA-256:
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
PE x86, base `00400000`, timestamp `4F68A875`, image size `00DEC000`.
Its complete `.text` matches the private Ghidra sample already audited in this
branch. New assertions and five full-function fingerprints are in audit_camera.py.

- `00927020`: constructor sets vtable `00FF2800`, initializes matrices and
  camera fields. `00709270` is the corresponding vtable-reset/destruction path.
- `00985D80`, selection at `00985E3A`: owner `+3F4` supplies the strong camera
  reference. The receiver remains live during the view construction.
- `00986139 → 00D3A320`: near getter (`camera+5FC`), followed by far, FOV
  (`0095E280`, `+5F8`), aspect (`0053EA80`, `+5F4`), frustum (`009962D0`,
  `+620`), view, projection and combined VP getters from that same receiver.
  Copies populate render-view `+168/+16C/+17C/+184/+190/+0/+100/+40`.
- `00C813B0`: thiscall setter, one float argument, `ret 4`; stores `+5F8`.
  Perspective mode `+614 == 0` calls `00BA9150` and `004BE0C0` synchronously.
- `00BA9150 → 00A31FF0`: `+5F8` is full vertical degrees. The builder uses
  `00FB2E10 = pi/360`, tangent, `Py=cot(fov/2)`, `Px=Py/aspect`. Output is `+4C0`.
- `004BE0C0`: combines `+480` view and `+4C0` projection through `00C02F50`
  into `+500`; invokes `00607CD0` for the SAME camera's `+620` plane set and
  `00685050` for its bounds.
- The resulting render-view `+40/+100` reaches `00CA2150 → 00816D50`, then the
  context/model WVP path `0076F8B0 → 00571670 → 00B74720`. The final upload wrapper
  was observed in the earlier live mono sample; the new selected-camera address
  has NOT yet been correlated live to that sample.
- `00BCA310` is a proven CPU sphere/plane rejection routine (signed plane
  distance versus radius, false on separation). `00A6F295` calls it after
  `009962D0` on a camera reference. This is not yet proof that this call rejects
  the particular missing peripheral wall. Do not substitute angular selection
  `007BF850` or this CPU query for an object-specific draw/visibility witness.

## Offline validation and remaining test

- Diagnostic and production x86 builds: PASS.
- 7,126 camera/math/rollback/actual-x86-shim checks: PASS.
- 1,307 input-gate/site/recenter/support checks: PASS; 13 input and 13 camera
  pins match the installed file.
- Matched capture, Present readiness and transfer regressions: 36 PASS.
- Native audit/swapchain regressions: 13 PASS.
- Steam and PSVR2 for this candidate: NOT RUN; installation preserved.

Commands: `tools/frame-boundary-simulator/build-tracked-gameplay.ps1` (optional
`-Exe <installed exe>`); the same script with `-CameraCover`; Python unittest
discovery in `tools` and `tools/native-camera`; `build.ps1 [-DiagnosticCapture]`.
Capture metadata records the selected camera's P/VP and frame and requires both
GPU inverse-projection witnesses to agree before COMPLETE can be published.

Next approved trial must back up DLL/INIs, re-audit the EXE, verify installed
hashes and the two installed/restored log lines, then inspect unpaused gameplay.
Keep SubmitMode=4/StereoMode=1, 3076x3140 source/output and the tested fog bypass.
Do not adjust separation, materials or sharpening. Require visible peripheral
geometry plus both tentacles, tracked hands/weapons, gestures, recenter and
stable fusion before acceptance. A failed capture is not retried automatically.
Restore the previous DLL/INIs after the controlled test and check hashes.

Remaining limits: coverage is proven for the lens rays around the native camera;
large head rotations/translations still use the existing GPU 6DoF path and may
need a subsequent native pose/visibility correction. Animation/hand IK, occlusion,
LOD, secondary views and lighting under the wider native camera need live
validation. This is a real rendering candidate, not a claim that those defects
are already fixed in the headset.
