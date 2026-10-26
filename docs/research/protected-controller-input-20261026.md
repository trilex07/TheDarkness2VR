# Protected controller input candidate

Base: `85eea69`. Status: **IMPLEMENTED / OFFLINE VERIFIED**. No Steam launch,
installation change, headset acceptance or claim of restored native targeting.

The previous bridge rewrote the main EXE's delay/standard XInput import table
and its diagnostic installation depended on the unsafe native gameplay bundle.
The protected image guard correctly prevented both routes.

`Diagnostics/ImagePreservingInput=1` now admits a separate external observer of
the system `XINPUT1_3.dll` implementation. The absolute system-path export name
and ordinal 2 must resolve to the same typed `WINAPI XInputGetState` entry.
The conservative executable span must lie outside the protected main image;
both implementation and callback modules are pinned for process lifetime.
The native trampoline is published before MinHook enables the entry.

Installation waits for the existing real successful Present readiness gate,
outside DllMain. An unrequested, unready or refused protected installation
cannot fall through to EXE import rewriting. Native aim/hands/throw/FOV and
old Present hooks stay disabled. `HeadLookMode=0` prevents stick-based head
rotation from double-applying the later geometric tracking correction.

The existing OpenXR actions continue supplying both sticks, triggers, face and
stick buttons. Hysteretic analog grips supply stock LB/RB actions. This is
button-level input, not a controller ray/native pickup-selection correction.
Cached packets are now published/copied under a short SRW lock; no XR or
graphics calls occur while the XInput consumer reads its snapshot. Physical
controller input retains priority; unowned ports/statuses remain native.
The physical recenter chord is protected against concurrent overlay polling;
its camera-state update is queued to the render thread in protected mode.

The original `image-preserving-vr` profile is unchanged. A separate
`profiles/protected-controller-input/` overlay enables only the safe input
option and retains the protected stereo/head/FOV/resolution restrictions.

Verification:

- Actual extracted installer/forwarding regressions: 30 production checks and
  84 diagnostic checks PASS, including all protected admission combinations
  and refusal without IAT fallback.
- Own-process x86 system-XInput test: 20,032 checks PASS, including 10,000
  controlled packets, native status forwarding, idempotence and unchanged
  executable-section hashes of the test process. No Steam/game process used.
- Existing and new profile inheritance tests: 4 PASS.
- Production and diagnostic DLL builds PASS. Diagnostic DLL SHA-256:
  `ec67e16d5adb83e8af042c02698b46ede2a3e086bc9b8a47744488965b68d436`.

Remaining gate: MAIN-authorized actual gameplay/PSVR2 verification of engine
polling, both Sense action profiles, sticks/buttons/grips/triggers and focus
loss. Controller-aligned pickup/throw and hand animation remain later phases.
Rollback is the preserved baseline DLL/INIs; the new option defaults OFF.
