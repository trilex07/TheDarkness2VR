# PSVR2 haptics integration

Integrates Grok's `side/psvr2-adaptive-triggers` through `a86cf6c`, including its
dependencies `890f35b` and `dc550f9`, into MAIN's current input/stereo lifecycle.
No original EXE bytes, native static tables, driver configuration or Steam launch
options change. DirectRecordCopy remains OFF in the Steam candidate.

## Delivery contracts

- Left and right have separate OpenXR vibration actions, pulse delivery state,
  native shot serials, trigger commands and driver acknowledgement state.
- The already owned pistol fire adapter reports after the SHA-pinned final
  `CFBD60 -> C47570` consumer returns nonzero AL. `C47570` ends with `mov al,1`;
  the surrounding original `CFBBA0` no-shot path never calls this consumer.
  This is a firing-consumer event, not inferred ammo/hit confirmation. Recheck
  the selected avatar's live equipped fire receiver and smart reference before
  publishing. Preserve outgoing flags, integer/SIMD/FPU state and LastError.
- Known owned pistol actions use actual events, including empty/no-shot paths;
  trigger hold alone produces no firearm vibration. Other Grok weapon profiles
  retain input-driven fallback because they have no audited shot consumer yet.
  No claim of actual-shot coverage for those variants is made.
- Finite shot/strike/grab/throw/execution pulses are not restarted per input poll.
  Automatic fallback may renew only after the previous pulse duration. Trigger
  IPC suppresses identical packed states; the x64 backend independently dedupes.
- Release, pause latch, VR-settings input ownership, loss of focus/session and
  per-hand tracking loss turn off the corresponding effects. Explicit reset
  discards outstanding shot windows. Failed vibration submissions are retried
  at most every 100 ms. All requested OpenXR pulses are finite, at most 100 ms
  for current profiles, so runtime refusal of Stop cannot leave indefinite rumble.
- Tentacle resistance is intentionally absent; tentacle cues use grip vibration,
  preserving each equipped gun's trigger profile. The existing per-hand reload
  pulse remains per hand instead of muting both guns.

## Build and package

Run `tools/psvr2/build-tests.ps1` for the standard x86 proxy, actual-source x86
haptic host, x86/x64 policy, protocol/backend and x86-to-x64 fake-driver loopback.
The owned-bootstrap test overrides LOCALAPPDATA only inside its fixture process.
Both loopbacks verify 10,000 duplicate submissions produce no driver calls and
both triggers release on shutdown. Fake loaders stay under `tests/psvr2/out`.

Run `build.ps1 -DiagnosticCapture` for the preserved Steam input/hands profile.
Run `tools/psvr2/build-bridge.ps1` for the separate production x64 helper.
Install only that helper, the real licensed `psvr2_toolkit_capi_loader.dll`, and
the notices in `%LOCALAPPDATA%/Darkness2VR/runtime/psvr2`.
`[Input] AdaptiveTriggers=1` enables vibration and attempts the helper from this
absolute private path at first focused input sync, outside DllMain. A random
session token authenticates local IPC; helper lifetime follows the game PID.
Existing externally configured bridge environments are respected, including
explicit disable. Missing/failed Toolkit leaves triggers off without disabling
OpenXR grip vibration or game input. No new driver installation is performed.

Verify game EXE and candidate DLL hashes, back up DLL, all four effective INIs
and any previous helper files before deploying. Preserve every existing profile
key except the explicit haptic enable. Retain a hash-checked rollback script that
also removes newly introduced helper files when they had no predecessor.

## Validation boundary

Offline PASS covers native fire ABI/no-shot behavior, independent event routing,
input/gesture/hands/throw regressions, pulse dedupe/lifecycle, authenticated IPC,
missing/inactive backend, trigger dedupe and both x86 builds. It is not physical
controller or FPS evidence. Next authorized manual Steam/PSVR2 session combines
left/right firing, both tentacle actions, reload, menu/focus/reconnect and the
existing stereo/peripheral geometry/flicker/frame-pacing checks. Roll back on
crash, broken input/stereo or performance loss. Do not claim headset acceptance
before that consolidated session.

Full native menu laser control remains a separate open task: the retained pause
observer alone does not supply the menu input receiver or its display surface.
