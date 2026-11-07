# Handoff: stable gaze acquisition and tentacle throw

**Date**: 2026-11-07
**Task ID**: tentacle-gaze-20261107
**Role**: MAIN (user requested implementation and simulator tests)
**Branch**: codex/tentacle-gaze-stability-20261107
**Worktree**: C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107
**Base**: a8abc057, production DLL A9D487B1; diagnostic117230AC/main INI22862511
**Status**: runtime verification in progress; not integrated or accepted

## Request and cause

User clarified: target disappears or jumps. Both protected acquisition and throw
previously switched immediately from gaze to controller on an invalid/blink
sample; acquisition also consumed unfiltered single-sample gaze jitter. Confirmed
in protected_left_aim.inl, native_throw.inl and LatestGaze in eye_gaze.inl.

## Change

Interaction-only tracking-space direction filter shared by tentacle acquisition
and throw. 0.35-degree fixation deadband, 20-ms low-pass for small changes;
changes over10 degrees require a consistent new fixation for20ms (3-degree
confirmation cone). Retain last accepted direction up to160ms from the source
sample, then resume controller fallback. Duplicate consumer polls cannot refresh
age or confirm a spike. Pause/invalid tracking/context clears the filter.
Raw NIS/menu/wheel gaze is unchanged. Native collision/query ownership, release
speed/spin/W and guarded private native code remain unchanged. No EXE patch.

Simulator-only gaze uses Local\\Darkness2VR.SimulatorGaze.<pid>, selected only
after explicit Simulator/Enabled runtime override. Helper simulator-gaze-source
never opens or modifies physical bridge Local\\Darkness2VR.Gaze.<pid>.
Synthetic sample tests do not establish physical PSVR2 calibration/acceptance.

## Offline verification

- test/build_eye_tracking.ps1:51 checks PASS, including blink grace, long loss,
  deadband, isolated spike, sustained saccade, repeated sample, clock regression,
  NaN/zero/future/stale rejection and bounded smooth change.
- test/build_protected_left_aim.ps1:444 checks PASS.
- test/build_protected_left_throw.ps1:319 checks PASS; new actual native release
  case selects gaze, preserves speed/W and retains controller fallback.
- Native character contract:21 instruction checks plus whole consumer SHA PASS.
- tentacle-comfort fixture passes targeting checks, then fails its fixed-address
  camera-leveling allocation (known address conflict, not claimed as full PASS).
- Protected x86 compiled with Community MSVC14.51.36231 /O2 /Z7; linked with
  /DEBUG:NONE after both /DEBUG links failed LNK1101 (MSPDB140 version mismatch).
  Runtime DLL D72F8E1CBED5CE41D7F90B5775C6CC89A55F39CA0F69933D3A01021C4CA498EE.
  Only subsequent comment/docs edits differ from this compiled source.

## Runtime and recovery

Private lab test-output/tentacle-gaze-20261107; fresh four-file user-backup receipt
created and hash-verified. Runtime results and restoration to be appended.
