# Corrected protected XR profile: controlled Steam verification

Result: **PASS for gameplay XR layer delivery; HMD image visible confirmed**.
Full motion-controlled VR and image quality are not accepted by this test.
Tested `codex/image-preserving-present` at `d722e95`. Exactly one Steam launch,
PID34144/TID17476, then user shutdown with Steam exit code0. No automatic retry,
merge, renderer edit, DLL rebuild, native FOV/sizing or unsafe gameplay hook.

## Fresh package and unchanged DLL

New private package: `dist/image-preserving-xr-config-d722e95-20261026T125258Z/`.
Its INIs were freshly merged from the current installed game INIs and the
current committed profile files, not copied from the earlier test package.
Every non-overridden semantic value was compared and retained. Effective keys:
HeadGeometry0, NativeStereoHeadGeometry0, SubmitMode4, StereoMode1, OpenXR ON;
ImagePreservingPresent1 and BlockNativeImagePatches1. Native input/hands/aim,
head/tentacle geometry, FOV, sizing, legacy EXE Present and capture remained OFF.

SHA-256 of tested package:

- Unchanged DLL: `ee0f3ca28ef29993ddda786b03e6a01a58973c88ae024f5488360095b628da24`.
- Fresh main INI: `5199da43f90c6a6b56d62a0d55401d2d93f505d6251717b7c99f80855a7aa083`.
- Fresh diagnostic INI: `defdcefcc3306de9934a98399ea8888b540cf44a1f0873ae90a8448b0c1b18d9`.

Only SteamVR/fpsVR were active at preflight. Existing read-only SHA/PE/byte
validation passed for the installed original EXE. DLL and all four game/local
mod INIs were backed up and hashes verified before deployment. No other game
session was interrupted. Profile regressions:2 PASS; no rendering code changed.

## Actual gameplay and submission evidence

- Real additional swapchain acquired from00964CC1 and Present from00A94057.
  Last cumulative sample:22000 entries = forwarded = returned = D3D_OK;
  recursive/errors/statuses/fault0. Startup Reset succeeded; guard remained ON.
- Desktop observation and user participation confirmed loaded gameplay.
  Both eyes rendered from the existing same-frame replay: e.g. TrueStereo
  frame15000, eye0=1/eye1=1, overflow0; FrameContext15000/pair15000, layers1.
- All25 periodic samples during FOCUSED world gameplay had layers1; none had
  layers0. Longest consecutive sampled focused-world run: frame8700..13200,
  predicted display times spanning50.0964212 seconds. These are sampled logs,
  not a newly instrumented per-frame trace.
- Existing `xr submit ok` records show shouldRender1, layers1, mode4 and
  3dGameplay1. `renderTrackingValid=true` is proven by the unchanged SubmitXR
  admission condition required to reach layer1, rather than a direct variable
  print. No validation was bypassed or weakened.
- Existing XRCadence counts only XR_SUCCEEDED(xrEndFrame) with nonzero layers.
  Consecutive focused steady windows show89.6..90.0 successful layers/s.
  No xrEndFrame failure was logged. This is submission cadence, NOT a measured
  unique-simulation FPS or proof of a90FPS combat performance target.
- Zeros resumed after session transitions back to SYNCHRONIZED/headset inactive;
  there were no zero-layer focused-world samples in this run.
- Actual source remained1920x1071; eye caches and XR swapchains2676x2728 each.
  No resolution, sharpening, FOV or quality parameter was changed.

The user confirmed that VR imagery was visible, and reported unavailable
controller input/head rotation and low resolution. Input/head geometry were
intentionally disabled in this protected rendering test. Stereo fusion/comfort
was not separately explicitly accepted; do not infer full VR acceptance from
image visibility. These three remaining features/quality issues stay open.

No C0000005/00E129EA CRC-pointer recurrence was recorded. Three first-chance
E06D7363 C++ exceptions were recorded (one with native EXE context, two with
UIAutomationCore context). They were nonfatal in this run: rendering continued
and the user exited normally, code0. Their throw origin is not newly diagnosed
here; no exception suppressor or speculative fix was added. This bounded run
is not a general crash-free stability guarantee.

## Rollback and retained evidence

User closed the game; MAIN verified the process absent before restoring DLL
plus four INIs. Every original file hash matched; EXE still
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
Rollback PASS. The tested protected package is retained privately, not installed.

Private receipt root:
`%LOCALAPPDATA%/Darkness2VR/runtime/image-present-xr-config-20261026T125337Z/`.
It contains package/backup manifests, original audit, final tested hashes,
existing runtime/Steam logs, aggregate summary and rollback receipt.
Runtime log SHA-256:
`1c13650f06518a6aa891fe70bad47ca6f7f7f194d3a54e28655590e485c9976f`.
No full private logs, binaries, captures, game assets or new toolchain committed.
