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
zero procedural impulse and impact output. This does NOT establish that the
train or damage has no signal, or that the user did not perform the test.

### Timing correction after user confirmed the test was already completed

User explicitly says the test was performed. Compared native `[PerfClock]` QPC
markers against a current QPC/UTC pair from the same Windows boot. The native
478-sample camera trace spans frames 1804..4217, about 20:40:39..20:41:09 UTC
(21:40:39..21:41:09 Warsaw). It records substantial movement: XYZ ranges
42.7077..43.9238, -7.2559..-7.2263, 176.2475..204.7839.
External capture files were created at 20:41:22 UTC and 20:42:46 UTC, respectively.
Thus the external capture began after the native 30-second trace had ended.
The unchanged second file cannot be used to dispute the user's performed test.
It is not proven exactly when train/damage occurred; the native trace has no
event markers. Game PID 38568 has exited by this follow-up. No further launch.

Preserved full native log as local `test-output/camera-native-session-38568.log`.
Analysis of 478 matched camera/root samples: camera path length 31.1595 game
units; peak camera/root separation 0.12131 at ms=3079/frame=2053; peak forward
angle difference 2.69505 degrees at ms=20672/frame=3463. These prove motion in
the earlier interval, but do not identify individual train/damage events.

Sampler header now includes explicit UTC and perf_counter start values so
future native QPC logs can be aligned without relying on file creation times.
For another user test, arm capture and verify it is actively recording BEFORE
asking the user to trigger an event; use a sufficient window (up to 300 seconds).

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

## Successful event capture and haptics candidate (subsequent test)

User says the game was never closed. Earlier sandbox `Get-Process` did NOT
reliably expose the user's game; do not infer process exit from that observation.
Use elevated `Get-CimInstance Win32_Process` / exact pinned PID for current state.

Reran recorder on still-live PID 38568. File
`test-output/camera-event-20261101T214956250.jsonl`, header UTC start
`2026-11-01T20:49:58.027013+00:00` (21:49:58 Warsaw), 300 seconds,
26,532 samples. User sequence: unpause, steps, train passes/look, approach enemies,
damage, one tentacle swing, further damage, pause. Activity ~31..57 seconds.
Source DLL remains `7414984F...`; the event trace is on the restored baseline.

- Exactly one candidate (`3F82EC30`) shows substantial output; four remain idle.
- Impulse `+20` of all four procedural blocks stays exactly zero for whole test.
- Active owner's impact position output peaks 1.39402; CA0 environmental
  output peaks 1.19662. CA0 is sustained ~33..40 s while movement/view block
  drops to zero; later impact pulses ~41..56 s remain after CA0 stops. Association
  with train and subsequent damage follows user ordering, not native event IDs.
- Live read-only chain independently verified: active owner `+828 -> 2767F234
  -> 2E2968B0` (controller table `00FE3EE8`), then controller `+74 -> 281253C0
  -> 3FAA93F0` (selected avatar `01001630`). All four idle owners have an empty
  `+828` reference. Stock getter `0043BCB0` returns controller `+74`; updater
  `00631560..00631572` uses this same chain. This resolves producer ownership.

Implemented default-OFF `Diagnostics/CameraEventRumble`: read-only selected
producer matching via that chain, impact outputs `+12F0/+1300` plus CA0 outputs
`+CA0/+CD8`, finite-value/expiry/unique-owner checks, native pause gating, existing
OpenXR finite 100 ms feedback delivery. It excludes walking/view `+8C0`, avoids
camera residuals and does not suppress real impacts merely because stick turns.
No native memory write is introduced. Rejected stabilizers must remain OFF.
Camera shake itself is still unresolved; this candidate addresses haptics only.

Tests: `test/build_camera_event_feedback.ps1 -Replay test-output/camera-event-replay.txt`
PASS, 50,624 checks. Replays 26,532 actual samples; 24,064 quiet/initial-step/paused
samples give zero, 417 train-window samples and 353 later-impact samples active.
Ownership refusal, wrong-avatar and NaN/Inf checks pass. These are offline
signals, not physical controller acceptance or native pause integration proof.

Build: initial BuildTools 18.6.2 link failed with MSPDB140.DLL version conflict;
in-place VS18.8.2 retry hit LNK1114 access denied on old d3d9.lib. Isolated,
unsandboxed VS18.8.2 build through `tools/build-camera-event.ps1` succeeds in
`build/camera-event/`; no global toolchain or original build script modification.
Final candidate also avoids periodic whole-memory rescans while its verified
producer remains valid. Deployment helper
`tools/deploy-camera-event-rumble.ps1 -CandidateHash <completed DLL SHA>`
supports read-only preflight; `-Install` refuses a running game, verifies current
baseline and nine backups, changes only DLL plus the event INI key, preserves
shotgun/guard and refuses all rejected shake flags. No new DLL is installed yet.

Final isolated x86 diagnostic build PASS, DLL SHA256
`8DB11F12A5032BCAC7FA4806C22E1439DDE63BE5035B244159FDD043EF1A8966`.
Read-only deployment/profile preflight PASS, all rejected shake flags OFF,
shotgun retained. Installed DLL still `7414984F...`. Full capture SHA256
`4F1786D5FD6F5CFD82003DA6007EA1D5E9B222C85329963CA087AAF57C7B845C`.

## Next

1. Physical haptics acceptance pending: steps/stick turn should be quiet; train and damage should produce feedback. Camera shake remains unresolved.
2. Inspect `[CameraEventRumble]` runtime evidence after user test.
3. For camera suppression, obtain a bounded hardware
   write watchpoint trace; do not recycle render-time zeroing or delayed filters.
4. Implement and validate only after ownership/address/instruction evidence.

## Installed haptics-only candidate (22:03 Warsaw)

User explicitly confirmed game closed and requested installation. Installed source
`63adb43b5d6130dc83757c9a328aed3e675a3176`, DLL SHA256
`8DB11F12A5032BCAC7FA4806C22E1439DDE63BE5035B244159FDD043EF1A8966`.
Diagnostic INI SHA256
`D218DB73AF6452218E9AE2829767869304F05EE294A3A88354EC00385FE963F3`,
`CameraEventRumble=1`. Deployment process checks confirmed closed game.
Independent audit verified all nine backup hashes, seven other protected targets
unchanged, exact installed DLL and stock EXE hash. Shotgun and native image guard
retained; rejected camera paths remain OFF. Agent did not launch game.

Rollback (only with game closed):
`C:/Users/Trilex/AppData/Local/Darkness2VR/packages/camera-event-rumble-20261101T220349965/rollback/Restore.ps1`.
Package `deployment.json` records the source and installed hashes. Physical
controller result NOT RUN; do not claim camera shake suppression.
