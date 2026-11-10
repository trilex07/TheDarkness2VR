# Optional double-shake reload — separate product candidate

Accepted source parent2567d04072e9a38c66faec017e3b2779471935e5.
Worktree `.double-shake-reload-20261110`, branch `codex/double-shake-reload-20261110`.
No Cinema/A/B/C/horizon changes mixed. Installed39D and all user files preserved.

## Product behavior and native ownership

`[Input] DoubleShakeReload=0` is defaultOFF. Opt-in1 recognizes two shakes with
either hand that has a fresh native firearm. Baseline physical button routes,
old hip reload, tentacles, support and heart controls are untouched. It produces
only stock X through the existing shared gamepad reload path. Hand identity also
feeds the existing reload haptic suppression. Native game still decides whether
and which weapon can reload: no native calls, ammo writes or fabricated ACK.

Each hand independently needs neutral speed<0.35m/s, then four alternating
powered strokes on its first dominant tracking-space axis. Each requires axis
speed>=1.2m/s for>=35ms, completed sequence<=0.9s, gap<=0.35s, sample gap<=150ms.
After firing, 1s cooldown and fresh neutral arming. Repeated same-time samples,
one-way motion, isolated spikes, invalid velocity/time, reacquisition or ownership
loss cannot finish a queued gesture. These are provisional gesture parameters,
not an invented throw loft/speed or measured headset calibration.

Wrapper requires current tracked grip AND aim, current Motion sample, released
grip<0.25, both triggers<30, no held buttons, focused XR, real game context,
protected input path, no menu/wheel/cinema/heart/support ownership and native
CopyNativeGameplay<150ms with Empty/no character/alternate hold. Native firearm
index0 is right and1 left; XR hand0 is left and1 right, explicitly mapped.
Context loss cancels both the recognizer and its own120ms pulse immediately.
The existing hip pulse has separate storage and is not erased by defaultOFF.
ResetPoseInput already resets the containing gesture state for owner changes.

## Existing optional reload-button mapping

The accepted source already exposes `[Input] AButton=X` (default) in
`weapon_wheel_input.inl::PhysicalAButton`. Optional `AButton=A` passes the stock
native A action from physical A/Cross instead. Thus a user who enables gesture
reload can choose stock held-object action on the old reload button with this
existing setting; no new hook or guessed native throw routine is required.
Menus/choices and alternate character holds keep their existing native A route.
Physical left X/Square remains X. No default or deployed profile is changed.
Actual stock prop quickthrow outcome/arc/fence reach is NOT YET VERIFIED; this
documents a proved producer mapping, not a native consumer acceptance claim.

## CPU verification and limits

`./test/build_double_shake.ps1` builds a small x86/O2 CPU fixture. Actual production
DoubleShakeReloadGesture is extracted from d3d9.cpp, not rewritten in the test;
only native sample/tracking/window context is faked. Fresh run: **862 checks PASS**.
Both hands/all3 axes; incomplete histories not combined;21 individual ownership
refusals; each refusal clears a pending pulse; Busy/Unknown/character, wrong-hand
firearm, stale/invalid motion, repeated predicted time, no neutral, no reversal,
sub35ms noise, NaN/infinity/time reversal/gap, cooldown and duplicate fire.
No actual native gameplay/reload execution is simulated by this fixture.

Product Release build/freeze is a separate gate recorded below when complete.
Runtime/visual/native ammo ACK/headset acceptance: NOT RUN. Keep task OPEN.
No horizon root-cause fix is claimed; raw RB/LOOK_Y binding remains unresolved.

Later separately granted gameplay test: unchanged baseline buttons first;
DoubleShakeReload1 only candidate, one reload each armed hand with actual native
ammo/action outcome and both-eye visual; negative menu/support/grab/heart; compare
optional AButton=A on a held harmless prop only under explicit scene authority.
Restore the session's CURRENT DLL/INI/settings/saves/XR/Steam, not old saves.
Do not merge with Cinema or iron-bar candidates before independent acceptance.
