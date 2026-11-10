# Aim indicator and native menu audit: source-first review

Branch codex/aim-indicator-20261110, isolated .aim-indicator-20261110.
Exact parent6a6f1f2b8ef9c4630df28074af7c73623fef771a is installed81FD's producer.
No game, process input, XR/GPU, lease or installation. MAIN owns deployment.
Preserve working double-shake reload and the user's three opt-in input flags1.
User headset acceptance confirms reload only; horizon remains OPEN.

## Fresh user evidence, not old simulator/title evidence

Private captures/aim-indicator/user-session/d3d9.log is550281bytes from PID7288,
SHA256 D70520F6DBF1BBD7361A042E47A9F41AA8B117435F90E10543E38BC861FF296B.
It contains actual gameplay ownership, right/left ProtectedWeaponShot stages32/33
applied1 and three right-hand DoubleShakeReload pulses. It starts with legacy
WeaponLaser0, then action8 enables it. Do not infer every shot/pose matched:
protected shot reports are rate-limited and contain no full direction/impact pair.

Installed81FD was freshly hashed unchanged. Current mainINI at this read was
761F827A9FCDC998DA510EC344E20A6C84BC50187787DD13588E3C44D486AF60
(after user UI changes), NOT a guarantee that earlier E317/228625 remains current.
Future rollback must back up CURRENT again and preserve later user settings.
No installed INI was copied, edited or restored in this task.

## Gameplay producer/consumer: what is proved and what is not

src/weapon_laser.inl calls CopyProtectedWeaponHandPose -> vr_gameplay_options.h
WeaponBeam -> vr_settings.h Laser. The finite beam ends8metres along the XR
hand/support aim. It is a direction preview, not a native collision/impact marker.

src/protected_weapon.inl ResolveProtectedWeapon validates live selected owner,
equipped gun/fire/model, current render/thread/tracking and gun-reference identity.
It uses the same controller/support pose but composes TrackingPose * inverse
recenter reference * StabilizedCameraWorld via BuildScaledWeaponMatrix.
src/native_weapon_bridge.h FireEntry passes the invocation-local world matrix to
C47570 exactly once, leaving native spread, collision, ammo and damage intact.

Pinned EXE D46BB6CB: C47570/35D digest223bfc40... matches source plan.
C47743 calls B27AA0 for spread, then C4776A calls AE9B50 for collision.
B27AF1's zero-cone branch copies matrix+20 (forward row) to the returned direction;
nonzero spread constructs directions around that row. This excludes a guessed
row0/row1-forward correction. Native spread/near obstruction can separate impacts
from an8metre preview, but the user-reported magnitude/cause is NOT established.
No invented muzzle offset, zero-spread override or collision call is introduced.

Actual beam geometry fixture now verifies240 nontrivial yaw/pitch/roll rotations:
beam local+Y is collinear with the hand ray and the Dot endpoint lies on that
same ray8metres out. This is negative evidence against a simple beam-quaternion
transpose bug. It is NOT a native-world/eye projection or collision acceptance.
A shared final shot-ray/renderer-space comparison still needs same-frame evidence;
do not claim the gameplay laser misalignment is fixed by the Dot preference.

## Native game menus are separate from the VR Settings panel

src/eye_gaze.inl ControllerMenuHit and ApplyGazeMenuCursor project the submitted
menu surface using g_eyeCache dimensions; target coordinates use2676x2728 in
this user log. Native position saturates at1934x1071 despite widened mouse bounds.
Examples: uv(.545,.764)->desired(1459,2084), actual(1465,1071);
uv(.795,.727)->desired(2127,1982), actual(1934,1071).
The saved native clamp is1935x1072, restored when leaving the menu.
Controller laser drawing is src/controller_menu_laser.inl; actual VR Settings
uses its OWN640x360 plane and round-trip UI fixtures, not Flash coordinates.

Prior bounded native proof remains relevant, not silently changed: Driver+48
owns input, movie+7C -> input+9C -> raw mouse+21C/+220; movie virtual+148 ->
6D56B0 scales with movie offsets/scales; CEDC30 sets those scales from native
extent/viewport. Manager size comes from GraphicsSys/50D440 surface/target
selection, not necessarily physical HWND client. Therefore real clipping and
coordinate mismatch are confirmed, but substituting client size for Flash size
without observing the actual movie viewport/scales is NOT yet justified.
Old BFE source-pixel/client assumptions are not transplanted or accepted.
No clamp weakening, new cursor mode or native menu memory write is added.

## Separate one-behavior implementation: Off / Laser / Dot

Existing Controls row/action8 now cycles0Off ->1Laser ->2Dot ->0Off; labels in
Polish/English. New VRSettings/AimIndicator defaults to legacy WeaponLaser0/1
when absent, preserving an existing OFF or existing ON. Explicit invalid values
fail to0. UI writes only AimIndicator, never rewrites legacy user keys/buttons.
Failed persistence restores prior state. No input gesture/native hook is changed.

Dot uses the same finite8metre aim point as the beam; it is explicitly NOT an
impact marker. Round16x16 alpha swatch uses its own region in the existing
independent renderer swapchain. Existing acquire/wait/release and both-eye layer,
menu/focus/tracking/weapon-owner/capacity gates remain. No ray for unknown weapons.
Laser retains its single solid texel; no new GPU path, shader or native trace.

CPU verification: `./test/build_vr_settings.ps1 -OutputDir <ABSOLUTE>/test-output/dot-final`.
Fresh7fixtures PASS:35,5789,2015,50,7215,68,20207 checks. Actual renderer covers
all3 labels in both languages, alpha swatch and resources; preference fixture
covers legacy fallback, explicitOFF, persistence, invalid modes and failed writes;
layer/geometry fixture covers both modes and all previous ownership gates.
One temporary duplicate fixture variable caused a compile error, corrected by
scoping the repeated swatch assertions; no production workaround.
Private logs test-output/dot-cpu/final-run-v3.log. Product Release build/freeze
follows this source review; no runtime/headset acceptance inferred.

## Proposed bounded next emulator scenario, requires MAIN grant

Back up fresh CURRENT81FD + current INIs/diag/loader/log/config/saves/absences;
protected Steam/XR read-only receipts. Own exact PID/creation/HWND, own atomiclease,
A76 process-local only, input A+360 and cleanup/release A+480 (close early350).
Preserve all three user option1 values; do not overwrite with repo profile.

First prove native menu producer/consumer with an actual visible native menu,
paired native HWND and both-eye image, real externalclient/clip receipt and ONE
previously reviewed read-only native Flash receipt if explicitly admitted.
At most3 hover targets; no blind clicks or image-age-only retries. Compare current
viewport/scales/native cursor to expected projected pixel before proposing fix.
The mod's640x360 panel is a separate test of Off/Laser/Dot labels and save/release.

If MAIN instead grants world-ray diagnosis: actual world before action; retain
static HMD, one eligible gun/hand and an unobstructed static wall; native ammo
and actual impact images before/after ONE released shot per admitted hand.
Archive both-eye/native image plus current ownership, head/reference/aim and
accepted shot matrix/direction if an existing qualified receipt provides them.
No new observer/hook or unknown-address read is authorized by this recipe;
omit unqualified data, report limitation, never invent ray/impact equivalence.
Normal owned close and fresh CURRENT restore by deadline, no final retention.
