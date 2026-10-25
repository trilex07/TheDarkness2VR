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

## Controlled bc47dab trial and input follow-up

The user explicitly approved one full VR trial of bc47dab. Installed EXE SHA/PE
and 35 instruction sites/29 ranges passed a fresh audit. DLL and both game INIs
were backed up before the single Steam launch. Diagnostic DLL SHA-256 was
`64b1c40feb4af6ab1cfa2ba6734ca57d083f52160e006242ceeab5f3908aef9f`.
The trial ended through the game menu. All three original file hashes, the
separate LocalAppData INI and the unchanged EXE hash were verified after restore.

**PSVR2 result, user report:** objects no longer disappeared at the sides in the
tested scene. The sticks did not move/turn the player. This is a visible
peripheral improvement, not complete acceptance of controls or every head pose.

**Runtime evidence:** the selected gameplay camera changed from vertical FOV
60 to 123.984 degrees at aspect 0.979618. Its native P coefficients were
`Px=0.542955637, Py=0.531889021`; both eye GPU inverse-projection witnesses agree
in recorded frame 25308. Eye scaling is `(1.00000012, 1.41468644)` and crop `(1,1)`.
Native rebuilt projection/VP/frustum messages continued through gameplay.
Thirty-four stale-camera refusals occurred around transitions; the guard withheld
the world layer on those frames. The last full Present snapshot had 34,000 equal
entry/forward/return/OK counts, zero status/error/recursion; detach recorded
34,736 calls. Do not extend the last full HRESULT snapshot to the unlogged tail.

The single capture is **INCOMPLETE**: two 3076x3140 RT0 sources, 814 draws per eye,
image mask 3, no final-eye images or COMPLETE.json. OpenXR was not submitting a
visible world layer around the request (session state 3/layers 0). The checker
rejects it. It was not retried. The native/GPU matrix agreement is partial source
evidence only; no Luna final-eye sharpness result is claimed. A future request
must verify active XR rendering immediately before arming, not just wait a fixed
number of seconds after the user's message.

### Actual defects corrected after the trial

1. `TryInstallXInputHook` still returned unconditionally under
   `D2VR_MATCHED_CAPTURE`, despite `RestoreTrackedGameplay=1`. Nonzero XR stick
   packets and generated grip pulses were logged, but no XInput IAT installation
   occurred. The existing bridge now installs immediately after the verified
   gameplay opt-in succeeds at CreateDevice. Earlier DllMain/factory calls and
   the default isolated diagnostic configuration still return without consuming
   the install attempt. No new native hook or controller remapping is introduced.
   The restoration log includes XInput status; bounded diagnostic messages show
   the first engine poll and first active XR packet actually forwarded.
2. A first-chance null read occurred in the old weapon hook. The **exact tested
   DLL/PDB** resolves RVA `2B0C3` to `TryNativeWeaponMatrix`; disassembly is
   `cmp dword ptr [esi],0FD6BC0h`, with logged `ESI=0`. A null model matched a null
   `entities[]` slot and reached the vtable read. An early null model/output guard
   now returns before slot matching. The existing SEH was catching the fault;
   that does not make it a valid successful trial. Two separately logged
   E06D7363 records match the previously investigated native bridge signature.

The runtime also logged pistol model updates and `NativeHands` wrist/finger/IK
application in both free-left and support states. Those logs are not a physical
acceptance of every hand or gesture interaction. The missing XInput bridge also
explains why gesture button pulses could be generated without reaching the game.

### Follow-up validation

- Diagnostic and production x86 builds: PASS.
- Real installer, input-forwarding and weapon-matrix function bodies compiled
  against synthetic providers: 29 diagnostic + 28 production checks PASS.
  Coverage includes pre-verification isolation, delayed opt-in, one install,
  import fallback/failure, unchanged VR stick/trigger/button packets, physical-pad
  priority, disconnected tracking, null model rejection and a valid tracked model.
- Existing capture/Present/transfer suite: 36 tests PASS. The actual partial
  capture is correctly rejected, not counted as a successful capture test.
- Follow-up diagnostic DLL SHA-256:
  `b8862093976ffcd3db0c74c7b2beb0a6703128dd04b014e76f00f4c7a88d02f2`.
- Steam/PSVR2 for the follow-up fixes: NOT RUN. The approved single trial was
  consumed; the game remains restored. Native FOV math, stereo, fog correction,
  resolution, crop and experiment INIs are unchanged from the tested candidate.

Run the new source-body regression with
`tools/frame-boundary-simulator/build-xinput-bridge.ps1`. Private trial logs,
captured buffers, symbols and rollback receipts remain outside Git.

## 6008c1e controlled input trial

The user subsequently approved the deferred trial. Fresh EXE identity/site audit
and candidate hashes passed; no other Steam game was running. One launch used
the unchanged experiment INIs with capture OFF. The XInput Delay-IAT installer
reported success, followed by an engine port-0 poll and an active XR packet
actually forwarded. The user confirmed locomotion, turning and gesture attacks
work. The pistol followed the controller and could be aimed independently.

Physical acceptance is **PARTIAL**, not a complete gameplay PASS:

- Human arm models remain frozen in the centre despite 23 logged native IK
  applications. CPU palette writes alone do not prove visible GPU skin updates.
- Throw direction still differs from the user's intended controller direction.
  Four of nine armed tokens reached the verified ordinary-object velocity hook;
  five additional requests had no eligible held entity. Successful applications
  changed native velocity while preserving speed. Do not label all throws
  camera-directed or relax entity/branch checks without identifying the skipped
  cases. The implementation currently blends controller aim with swing velocity
  at assist 0.75.
- User reported some image instability and intermittent small artifacts. Heavy
  segments logged roughly 47–55 submitted layers/second and 12–17 ms D3D9 GPU
  time, excluding D3D11/XR. This supports investigating frame-time pressure, but
  does not establish the cause of every artifact or justify head-pose smoothing.
- User requested tentacles follow head rotation; this is not implemented by the
  existing world-relative tentacle pose. Keep independent hand/weapon aim intact
  when designing that behavior.

Last full Present snapshot: 7,000 equal entry/forward/return/OK counts, no
recursion, status or error; detach count 7,083. Two first-chance E06D7363 records
had the previously recorded 91930520/EEA1D7 bridge signature. No C0000005 null
model fault was recorded in this trial; this is bounded evidence, not lifetime
proof. Game exited; DLL, both game INIs, LocalAppData INI and EXE hashes match
the preserved baseline after restoration. No capture or second launch occurred.

Next targeted work: connect the written human palette to the actual visible skin
upload, identify unconsumed throw-token branches, and measure the heavy rendering
passes before changing resolution or filtering. Native camera coverage remains
the previously headset-tested improvement; do not reset its settings to chase
these separate control and frame-time defects.
