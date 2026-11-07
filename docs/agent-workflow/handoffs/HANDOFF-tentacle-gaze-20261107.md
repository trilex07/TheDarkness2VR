# Handoff: stable gaze acquisition and tentacle throw

**Date**: 2026-11-07
**Task ID**: tentacle-gaze-20261107
**Role**: MAIN (user requested implementation and simulator tests)
**Branch**: codex/tentacle-gaze-stability-20261107
**Worktree**: C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107
**Base**: a8abc057, production DLL A9D487B1; diagnostic117230AC/main INI22862511
**Status**: simulator verification complete; production packaging; not integrated or headset accepted

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

Private lab test-output/tentacle-gaze-20261107; own Steam PID43428, UTC start
ticks639296530715404800. D72F8E1C profile/flavor gate and lab installation PASS.
Simulator producer used isolated section; real PSVR2 mapping was not touched.
Title/Continue -> crane gameplay, two tracked controllers. Actual private
camera-getter acquisition consumer applied the published ray20 logged times.

game-final.log / verification.json:569 filter observations;40 short dropouts
retain gaze,39 pending-spike observations retain the preceding fixation,
140 observations accept the deliberate new fixation.120 longer invalid periods
correctly use controller fallback. No claimed object/entity hard lock.
One native LB grab pulse delivered; no attachable target at that view. Full
held-object flight NOT VERIFIED in this simulator run. Release direction and
preserved physics are covered by the319-check actual-body offline test.

Fresh 20.082-s frame capture:1710 frames,85.15 FPS,p50=11.595/p99=15.426ms,
56.9% above11.111ms,renderer92.7% core. No matched baseline A/B; no FPS claim.
world-left.png and grab-gaze.png retain the normal rendered scene.

Stopped owned game after path/start-time check. simulator-lab Restore PASS:
DLL A9D487B1, diagnostic117230AC and protected EXE/main INI/loader unchanged.
Four current pre-lab save/config files restored and SHA checked; post-test
versions preserved under post-test-user/. Simulator OFF after recovery.

Frozen DLL-only package uses kind tentacle-gaze-stability and existing guarded
installer with exact baseline/candidate hashes and protected profiles/caches.
Prepare script pins the simulator-tested D72F8E1C bytes. No runtime INI changes.
