# PSVR2 eye tracking: recon for gaze interaction and foveated sharpening

MAIN offline recon, 2026-11-01 00:10 Europe/Warsaw. No game launch, no code change.
User decision: no dynamic foveated *rendering* now (D3D9 has no VRS, an inset
costs extra replay passes on an already CPU-bound frame). Build instead:
(1) gaze-assisted interaction, (2) gaze-centred (foveated) NIS sharpening.

## Data source (verified in references/KHARVOX/tools/PSVR2Toolkit)

- Public C API (`psvr2tk_capi.h`): `bool psvr2_toolkit_gaze_status(hmd2_gaze_status_t*, uint32_t timeoutMs)`.
  Same loader/module we already load in the x64 bridge for adaptive triggers
  (`src/psvr2/ToolkitBackend.cpp`: `psvr2_toolkit_init`, `set_trigger_effect`).
- `hmd2_gaze_status_t` (0x148 bytes, `common/hmd2_gaze.h`): `wearable` block
  (per-eye origin mm + normalised direction with validity flags, combined
  origin/direction, blink, pupil diameter, convergence validity, timestamp,
  frame counter) and `foveated` block (tracking_state, per-eye + combined
  direction, convergence distance). Requires the user's Toolkit gaze calibration.
- Internally the driver publishes it in shared memory `CUSTOM_SHARE_VRT2_WIN`
  (`libcustomshare`) with a counter; we do NOT read that private layout, only the C API.
- Licence: Toolkit eye-tracking data must not be used commercially; this mod is free.

## Transport into the x86 game DLL

The current pipe is one-way (`PIPE_ACCESS_INBOUND` in `BridgeMain.cpp`, client
opens `GENERIC_WRITE`). Plan: the x64 bridge polls `psvr2_toolkit_gaze_status`
(timeout 0, ~250 Hz thread) and publishes the latest sample into its own small
named section `Local\Darkness2VR.Gaze.<gamePid>` (seqlock: even/odd sequence,
timestamp, combined direction, per-eye validity, blink, tracking_state). The
DLL maps it read-only; stale (>100 ms), invalid or blinking -> no gaze, every
consumer falls back to today's behaviour. No pipe protocol change, no new
dependency in the game process.

## Unknowns to verify in the headset (counter-predictions)

1. Coordinate frame of `gaze_dir_combined_norm` (expected: head/HMD space,
   +Z or -Z forward). Test: a debug dot quad at gaze in head space; looking at
   the four corners must move the dot to the same corners. If mirrored -> flip axes.
2. Update rate and latency (log frame_counter deltas per second).
3. Whether `foveated` is populated without the Sony foveation path (log
   tracking_state and validity counts).

## (2) Foveated NIS sharpening - design

NIS is compiled with `NIS_VIEWPORT_SUPPORT=1` (`nis_runtime.h`) and the config
already carries input/output viewport origin/size. Per eye:
1. existing full-frame dispatch with the base sharpness (unchanged path);
2. one extra dispatch over a gaze-centred rectangle (default ~25% of the eye
   width) with the higher fovea sharpness, writing the same output texture.
Gaze -> pixel uses the per-eye pose/FOV we already submit. Graded edge: two
nested rectangles (fovea, transition) to avoid a visible step. No new shader,
no native rendering change; cost = one small extra dispatch per eye (measure
with the existing NIS GPU timing). Needs a second constant buffer per dispatch.
INI: [Render] FoveatedSharpen=0 default (live A/B), FoveaSharpness, FoveaSize.

## (1) Gaze interaction - candidates

User choice 2026-11-01 00:12: **tentacle grab target** and **throw target**.
Weapon wheel and menu stay controller-driven.

- Tentacle grab target: when the grab gesture starts, prefer the object/enemy
  nearest the gaze ray instead of the controller ray (native throw/grab aim
  already accepts an injected ray: `native_throw.inl`, `protected_left_throw.inl`).
- Throw target: thrown object flies towards what you look at.
- Weapon wheel: look at the item, release R3 (alternative to hand movement).
- Menu pointer: gaze + trigger as a fallback to the controller laser.
Rule: gaze only *selects*; the action still needs the existing button/gesture.

## Order of work

1. Bridge gaze publisher + DLL reader + `[EyeTracking]` log (rate, validity) + debug dot. Headset check of axes.
2. Foveated NIS sharpening behind INI (default off) with GPU timing.
3. Chosen interaction(s), each behind its own INI key.

## Headset results 2026-11-01 00:26 (v5, DLL E5DEFFDF, bridge 639F1460)

- Bridge publishes ~60 samples/s, 100 % combined-valid, age 0-15 ms; foveated
  `tracking_state` stays 0 (use the wearable block, not `foveated`).
- Raw axes: looking ahead raw=(0.0,-0.07,1.0) -> +Z forward; Y correct;
  X mirrored (look left -> ring right). Fix: `GazeAxes=-x,y,-z` (now the code
  default; also written to the game-folder Darkness2VR.ini, backup
  `%LOCALAPPDATA%/Darkness2VR/Darkness2VR.ini.before-gazeaxes-20261101`).
