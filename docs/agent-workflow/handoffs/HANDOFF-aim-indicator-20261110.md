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
Private logs test-output/dot-cpu/final-run-v3.log. Product Release build/freeze completed below; no runtime/headset acceptance inferred.

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


## Frozen Dot-only product review packet

Product source494e2b43 (parent6a6f1f2b). Build `CL=/showIncludes;
./build.ps1 -DiagnosticCapture`, NO Install. x86 protected-native profile gate
PASS with CURRENT main/diagnostic INIs. Missing14 ignored build inputs were
copied only after matching the independent81FD frozen input manifest; no arbitrary
root edits or assets were copied. DLL/PDB CodeView GUID matches, age1.

Private immutable directory: captures/aim-indicator/dot-frozen-v1.

| Artifact | SHA256 |
|---|---|
|d3d9.dll|4C2A8EA69AB8A2785CAB64DEE3DEBA76612FBCBA61F7B9ED38DD6A3FDBCB2B79|
|d3d9.pdb|3509BA07344D9B7F6D3F26256D101B0FFB3F81118BB149F61CDF84A0E6350A95|
|d3d9.map|5361E42A76958BDDD6E3C60A8ADE0D720C6AA8C16E7FB7C5F2C4E6E2AD7DEFA4|
|manifest.json|D7A5158B382956392FF91415A1356EB68EFB7A068152A91986DD9624D27DCB4C|

Manifest pins570 build/include/tool/library inputs,438 tracked source/test inputs,
3 artifacts and3 evidence files. Its conservative dependency scope is explicit;
not a complete Windows process dependency claim. All records freshly verified.
Build log release-build.log, profile-gate.log, final7fixture log final-run-v3.log
are copied into this directory. No source change to reload/retry/elevation, no
ray/muzzle/menu behavior change and no candidate installation.

### Runtime proposal clarification for MAIN (no grant currently)

MAIN owns Wonderlands13348 until its cleanup; do not acquire before NEW grant.
Current81FD and the CURRENT user-modified INI, not old39D/E317/228625 bytes, are
the restore target for any diagnostic session. Keep Input/DoubleShakeReload,
DeferredGripRetry and ThrowSolverFailElevation1 unless user explicitly changes.

Suggested first bounded session8minutes total (input+360, early close350,
restore/release480) can review this Dot-only build's actual640x360 Controls row
Off/Laser/Dot with right-trigger release and INI outcomes. Also qualify the
actual native-menu dimensions/scale snapshot at an observed Main menu if the
unchanged639c read-only helper is expressly admitted. Dot does not change native
cursor mapping and cannot be called a native-menu or shot alignment fix.

For the actual world scenario, use observed native Title->Main transitions,
max2 released TitleEnter in20seconds; Main requires actual visible intended
selection before ordinary native A/Enter. At most one Main confirmation, then
fresh actual image. NewGame chooser/Quit/changed phase stops further confirms.
If world is not reached, record that limit and close early; no blind arrow loop.
If world is reached, static wall/HMD and at most one released shot per hand,
positive native ammo+impact and both-eye/native images. Keep gesture/retry/elevation
options1 untouched but do not exercise reload/bar in this laser-only scenario.
Without an already-qualified same-frame ray receipt, impact comparison remains
visual evidence only and must not be advertised as a measured native one-ray fix.

## Actual resolver CPU follow-up (no product rebuild)

`tools/build_weapon_indicator_ray.ps1` builds the existing actual x86 weapon
fixture and `tools/verify_weapon_indicator_ray.cpp`. Run from this worktree:
`./tools/build_weapon_indicator_ray.ps1` (CPU child only; no live game).
Result test-output/ray-proof/result-v2.log: existing125256 owner/bridge checks,
3200 exact native-body invocations, plus9600 additional assertions over640
camera-pitch/recenter-yaw/hand/scale cases PASS. Scales1 and100, both firearm
slots, nontrivial hand yaw/pitch/roll and recenter position are covered.

The test calls actual ResolveProtectedWeaponFire and CopyProtectedWeaponHandPose,
then maps the native row2 endpoint back through inverse camera and the scaled
recenter reference. This equals the XR Dot endpoint. StabilizedCameraWorld is
the existing fixture's explicit camera+80 stub: this is a transform composition
proof, NOT verification of the real stabilized camera, actual rendered eye
projection, native spread/collision, two-hand support or live sample timing.
The native camera/eye path and same-frame sample identity remain candidate
boundaries to measure, not established root causes. No gameplay ray fix follows
from this negative evidence.

Initial result.log exited after all original fixture checks without reaching
the added assertions: renaming C++ main to an ordinary int function removed its
implicit return0 guarantee. The new script generates a private fixture include
changing ONLY that entry signature to void and preserving every statement;
original pinned test/product sources and the frozen Dot manifest are unchanged.
This was test setup, not a game failure. No failure log is removed.

Latest coordination: Singularity preflight aborted; MAIN now owns Wonderlands
501e8088 acquired15:28:14Z for its bounded slot. Darkness is CPU-only until a
new explicit grant, with no pre-acquisition, install, GPU or process input.
The bounded8minute recipe above remains a proposal, not authorization.

## Coordination checkpoint and preflight, 15:35:48Z

HEAD5378c77d; branch codex/aim-indicator-20261110; clean worktree before this
checkpoint. Relative to installed source6a6f1f2b: five production files changed
(settings preference/config/renderer, gameplay Dot helper, layer mode selection),
three test files, two CPU proof tools and this handoff. No native menu mapping,
fire resolver, reload/gesture or throw source is changed.

Private captures/aim-indicator/preflight-cpu-v1/readiness.json records a fresh
read-only check: all33 existing main-a-review30 packet pins still match; installed
DLL81FDDEBA, mainINI761F827A, diagnostic117230AC, loaderD4DD992E, EXED46BB6CB
match the actual current files; DoubleShakeReload/DeferredGripRetry/
ThrowSolverFailElevation are each1. This is NOT a backup or runtime receipt.
MAIN reports501e8088 closed/restored29SHA/released. No Darkness lease acquired.

Exact outstanding preflight difference: the existing lifecycle adapter's
darkness_session_backup.PROFILE_HASHES still pins39D/228625/117230, and the
adapter candidate constant still pinsBFE. The old executable invocation is NOT
ready for the current installed81FD/mainINI761F827A or Dot4C2A8EA6. Before any
grant/acquisition, bind ONLY those exact current baseline hashes and the new
candidate/bounds in a private copy/in-process descriptor, as the S18 isolated
product binding did; shared helpers and old frozen packets must remain unchanged.
Recheck pins, actual CLI/import resolution and inert default before reserving.
If current files change, review the difference; never blindly refresh hashes.
No broad policy change or new runner framework is needed or authorized here.

Preparation gates for the next reviewed packet, in order:
1. Pin Dot4C2A8EA6/manifestD7A5158B plus unchanged A76 descriptor/settings,
   helper639c and the current lifecycle/focus/same-object UI helper bytes.
2. Privately bind the exact CURRENT profile above, keep mainINI byte-for-byte
   during installation and preserve input options1; diagnostic Simulator-only
   overlay remains process-local. Check every intended command's parser and
   dependency imports inertly before acquisition. No historical session root.
3. After a NEW explicit grant only: atomic FREE/no-games acquire, publish A/token,
   inputA+360, early close350, cleanup/releaseA+480. Fresh bounded inventory and
   backup CURRENT mutable state+absences, EXE/loader/XR/Steam protected receipts
   and deadline check before the first mutation/launch.
4. ONE reviewed scope: Dot settings-panel cycle, or actual native-menu scale
   diagnosis, or world-ray comparison as detailed above. Do not silently combine
   extra controls when a gate fails. Read-only Flash639c needs explicit admission;
   no new hook/collision call, clamp relaxation or cursor-mode change.
5. Owned normal close, exact fresh restoration and absence/new-file checks,
   independent hashes/protected state/no-games verification, release. Exit code
   remains UNKNOWN unless a real retained process handle yields it. Installed
   rollback target is CURRENT81FD and CURRENTINI, never historical39D/228625.

Build/review ready: Dot-only product. Runtime orchestration readiness: blocked on
the narrow private current-profile binding and its inert invocation verification;
do not consume a slot to discover that stale baseline refusal. Product cause
blockers remain actual native Flash scales and live world/eye/impact evidence.
