# Handoff: native shake source and event feedback

**Date**: 2026-11-01
**Role**: MAIN (explicit user assignment)
**Branch**: `codex/camera-shake-source-main-20261101`
**Worktree**: `C:/Users/Trilex/Darkness2VR/.main-camera-shake`
**Source base**: `3c19e744`; inherited experimental code is NOT the installed DLL.

## Goal and status

In progress: zero forced camera shake, stable weapons, impact/train haptics,
no haptics from walking or stick turning. No working new fix is claimed.
User agrees to a train/damage test with read-only diagnostics.

## Completed

- Read bootstrap, actual MAIN baseline, failed approaches and predecessor handoff.
  Initial root checkout is an unrelated texture SIDE branch; leave it alone.
- Preserved predecessor's uncommitted handoff and FAILED-APPROACHES change here.
- Restored rejected installed DLL `A6BDC304...` through its exact rollback script:
  `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/camera-anim-stabilize-20261101T212812295/rollback/Restore.ps1`.
- Independently verified all nine restored files match manifest. Current DLL:
  `7414984FF998FABDF154AEB5EE443D75805378DDB2359F7EC6F20E83E8A6FA55`.
  EXE: `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
  Game was closed for restoration and was not launched by the agent.
- Added `tools/camera_shake_event_sample.py`: external VM_READ only, executable
  SHA pin, checks object types each sample, camera/avatar backlink check, bounded
  duration, exclusive output creation. No debugger, native write, new DLL or INI.
  Captures all candidate procedural amplitudes/factors/outputs, impact enable
  bytes and `+0x12F0/+0x1300` outputs, total shake, camera/parent matrices at
  approximately 100 Hz. Sampling is asynchronous and may miss short events;
  candidate objects do not establish ownership or native execution order.

## Static evidence checked in actual stock EXE

- `00C8AD43` reads event type at argument `+0x1C`. Type 1 adds DB8120's
  computed strength to owner `+0x930` (`00C8AD68/70`). Type 2 calls
  `009DFFF0` on owner `+0x1170` (`00C8AD9C..ADA7`). Other types add
  strength to `+0x8E0` (`00C8ADCB/D3`). Event names still unproven.
- `009DFFF0..009E0037` dispatches to two spring and two noise impulse
  functions, bases `+0/+0x70/+0xE0/+0x130`, via `00762DA0` and `00C778C0`.
- Old rumble samples only procedural `+0x20`; it cannot detect a type-2
  event whose sole effect is the impact subsystem. Old suppressor also clears
  impact enable bytes. This is a concrete blind spot, not yet proof which
  event type represents the user's damage or train scenario.
- Rechecked `00628420..006286BC`: amplitude = impulse+ambient+movement;
  rotation factors `+0x10/+0x14/+0x18`, position factor `+0x24`. Factor
  writes elsewhere remain unresolved. Full EXE linear scan found numerous
  overlapping offsets in unrelated types; offsets alone cannot identify writer.

## Capture

Command: `python -B tools/camera_shake_event_sample.py --seconds 120 --output test-output/camera-event-20261101.jsonl`.
Waits up to 180 seconds for game, then up to 180 seconds for gameplay candidates.
User should walk, turn stick, approach train, receive damage and report sequence.
No test result is recorded until the file is read and associated with user events.

Actual run: MAIN launched Steam AppID 67370 after the first process wait expired.
Game PID `38568`. Initial 120 s capture has 11,052 samples, but only 348 camera
samples: the existing protected aim path replaces getter slot 39 after admission.
Fixed the sampler to compare all 70 unchanged entries in the 71-entry camera
table against the stock table and follow each verified avatar's current camera.
This matches `protected_left_aim.inl` / `native_view_instance.h`; no game write.

Second file `test-output/camera-event-20261101-v2.jsonl`: 7,922 samples over
89.999 s, camera valid in every sample. Camera XYZ is exactly unchanged
`(43.58679962, -7.24896622, 176.25599670)` throughout. All five candidates have
zero procedural impulse and impact output. This is an idle/paused observation,
NOT evidence that the train or damage has no signal. User event confirmation is
pending; a new async question asks whether gameplay is still paused.

Validation: CLI help, invalid-duration rejection and non-game PID refusal before
output creation PASS; `git diff --check` PASS. No runtime fix was built/deployed.

## Additional static composition lead

On the exact stock EXE, metadata loader `004B389A..004B38B1` loads reflected
record `010B5348` (`mFPEntityIgnoresShakeTranslationAndRotation`) into owner byte
`+0xBD5`. Method `00809390` tests it at `00809A11`. The enabled branch uses
`+0x200/+0x204/+0x208`; the disabled branch at `00809A51..00809A7E` additionally
adds owner `+0x1F0` to the first-person position. Both branches build owner
matrix `+0x70..+0xA0`. Separately, `00809B47..00809BAF` still adds `+0x1F0`
to the camera position and calls `00ADDE10`; thus this byte alone is NOT a
complete camera-shake toggle. This is a useful native consumer lead, not a
proven safe intervention or proof of active owner identity.

## Next

1. Inspect actual capture completeness and user event sequence.
2. Establish active owner and which impulse/output changes during events.
3. If factor writer or transform order remains unknown, obtain a bounded hardware
   write watchpoint trace; do not recycle render-time zeroing or delayed filters.
4. Implement and validate only after ownership/address/instruction evidence.
