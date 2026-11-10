# Darkness focus checkpoint: native pitch, shot ray, menu event coordinates

Branch `codex/native-flash-alignment-20261110`, new isolated worktree
`C:/Users/Trilex/Darkness2VR/.native-flash-alignment-20261110`, parent07707890
after Dot494e2b43/source4C2A. The older aim-indicator tree remains unchanged.
CPU/source/math ONLY. No lease, game, GPU, install, XR or input; MAIN owns
Wonderlands runtime and all deployment. No running child/build remains.

## Current code, not a ready Steam fix

Opt-in `Input/NativeFlashRayCursor=0`: added `native_flash_cursor.h/.inl`,
submitted texture/crop metadata in `eye_gaze.inl`, Main-only mapping and guarded
confirm path, build/config inclusion. No gesture/reload/throw/Cinema edits.
The math maps actual submitted quad imageRect -> native render viewport ->
movie extent via the native float32 transform; it never substitutes client or
XR-output dimensions. Unknown offsets/scales, unsupported pause/HUD planes,
stale/tracking/phase/owner/foreground/resize mismatches refuse.

**Integration prototype BLOCKED, not deployable:** its atomic original static
Flash table slot publication is correctly refused by the current protected
NativeImageGuard. Do not enable this build or weaken the guard. This was caught
in source review before any install. The CPU mapping proof is valid independently
of that rejected publication design. No final installable alignment freeze yet.

Potential bounded product integration (NOT implemented/admitted): native43E160
tail-jumps to mouse virtual6C/603190, so its caller remainsCB7259 and the two
output pointers are the live borrowed movie+E0/+E4. This is a concrete pre-Flash
event recipient, not a generic cursor offset. Existing native_menu_mouse_poll.h
has the exact GetDeviceData/9A5E70 borrowed-owner gate; a correctly pinned private
mouse table could only be published during such a lease, not from a read-only
top-movie snapshot. Requires exact new production/ABI tests and review; no new
runtime or framework is authorized/being prepared.

## Actual CPU results and receipts

`test/build_native_flash_cursor.ps1` exited0; log
`test-output/native-flash-cursor-v1/build-run-v5.log`.
Legacy RED exit1: Flash(255,183) vs required(666,509) for the actual S21 values.
GREEN:51,095 checks, including51,005 complete native SSE body comparisons,
actual591650 MOVE event queue/button preservation, production callback/math/
renderer metadata ownership, crop, output independence and resize refusals.
Four complete method hashes/two slots/four rounding constants match local EXE.
Two actual production Guard controls reject both static-table and code patches.
CPU body copy only rebases four constant reads and one queue call; actual callback
fixture rebinds only original method address. Native updater and real XR not run.

All seven actual VR settings/gameplay/indicator fixtures PASS after this source
change:35/5789/2015/50/7215/68/20207, terminal0;
`captures/native-flash-alignment/settings-regression.log`.
Full protected x86 Release `CL=/showIncludes; ./build.ps1 -DiagnosticCapture`
terminal0, log `captures/native-flash-alignment/product-build-v1.log`.
This compiled prototype is review evidence, NOT a deployable/accepted fix.

## New actual user-log horizon evidence

Exact user log copied earlier in aim-indicator's ignored
`captures/aim-indicator/user-session/d3d9.log`, PID7288,550281 bytes,
SHA256 D70520F6DBF1BBD7361A042E47A9F41AA8B117435F90E10543E38BC861FF296B.
At15:03:54.309Z/frame8072 RecenterCapture3 reports native-camera
U=(.758904,.5,.417211), F=(.438153,-.866025,.240877), while avatar up=(0,1,0).
NativeCameraLevel on camera023D9BE0 explicitly logs pitch60->0, yaw61.2 unchanged.
At15:03:55.154Z/frame8141 RecenterCapture4 reports native-camera up=(0,1,0),
forward=(.876307,0,.481754), followed by native level pitch0->0.

This proves a real native base-camera pitch excursion and actual reset in the
user session, unlike the previously available historical November7 log. It does
NOT prove the original writer, continuous drift rate, a stationary HMD, or RB
gesture causality. These are click-frame CPU observations, not GPU synchronized.
All three logged ProtectedGesture right strokes occur later in this log;
do not attribute the earlier60-degree sample to them without the input chain.
Highest remaining target: the native camera input record passed as argument2
to B59D50 (EDI from caller), consumed at5F8C30 input+24, then BBD640 pitch.
Configured mapper789FF0's84-byte event record is not that typed camera record.

## Gameplay aim and completion checklist

Actual final shot resolver/owned native consumer proof already exists in
aim-indicator `tools/build_weapon_indicator_ray.ps1`: controller/support pose,
recenter transform and native forward row2 verified with actual fire body;
native spread/collision preserved. Its stabilization/camera fixture is synthetic,
not an observed world/view/shot triple from the user's report. No pitch clamp,
zero-spread cheat, muzzle offset or guessed visual rotation is implemented.

- Menu coordinate math/event consumer: CPU PASS; protected publication OPEN.
- Horizon native pitch excursion: actual user evidence YES; writer/root fix OPEN.
- Gameplay indicator vs final native shot/render: producer math PASS; cause OPEN.
- Working reload gesture, throw opt-ins, Cinema/buttons: preserved.
- Complete Steam candidate / final work freeze: NOT READY until required fixes
  or precise evidence blockers reviewed. No installable candidate claim.
- Current install freshly rehashed81FD/761F/1172 unchanged; gesture/retry/elevation1
  retained. Future MAIN-only install must back up CURRENT, never historical state.

## Bounded continuation after MAIN review of234d5e07

Only three product obligations remain; there is no further live/runtime or
native Flash orchestration work. No build is currently running.

1. Horizon: close B59D50 argument2 ->5F8C30 LOOK_Y ->BBD640 pitch writer.
   Current user evidence proves native pitch60->0 at recenter, not the input
   writer. A bounded .text search of direct camera+62C loads followed within170
   bytes by virtual+68 dispatch found zero matches; this is a narrow negative,
   not exhaustive proof of absent dispatch. Existing inactive-XR/reset paths and
   HeadGeometry1 exclude only the already documented simple local/head-stick
   latch. Preserve gestures/buttons; no pitch clamp or guessed sample timeout.
2. Menu: replace the rejected static-table prototype only if the existing
   borrowed mouse-call lease can support a fully pinned private instance table
   and actual getter callerCB7259/output movieE0/E4 ownership. Keep native
   transform/event/buttons, OS pointer and ClipCursor untouched. If safe
   publication cannot be proved, retain an explicit blocker rather than ship
   the compiled prototype. No new runtime helper framework.
3. Gameplay: compare actual production native shot composition against render
   inverse/base/reference for the SAME owner/scale/support/recenter case,
   including nonzero native pitch. Existing synthetic native-fire tests do not
   establish the user's visible mismatch cause. No visual-only offset guess.

Next checkpoint budget: one additional30-minute CPU/source slice, reporting
exact implemented source changes or a closed evidence blocker for each item.
That is a checkpoint estimate, NOT an installable-DLL promise. Candidate ETA
is BLOCKED on pitch-writer cause and protected menu publication; once those
are justified, reserve one bounded build/regression/freeze pass (about20minutes,
not including MAIN review). Do not silently expand the investigation or label
an unresolved issue fixed to meet an ETA. A frozen combined Steam candidate
must retain working81FD gestures/retry/elevation/Cinema and current userINI;
MAIN alone installs with a new CURRENT rollback, then SIDE freezes work.
