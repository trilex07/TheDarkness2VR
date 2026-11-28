# Darkness II VR gameplay and lighting follow-up

Date 2026-11-28. MAIN branch `codex/darkness-simulator-fixes-20261128`.
Branch base `2b101b98196e9a21fe15be3e206f2391ed14819b` matches the installed
protected-native DLL `E6B8B6058CBE07126753D0C799DD031CA81E152FAD3503D856B1AFA22A85A97B`.
The accepted `origin/main` remains the earlier crane/particle candidate
`9515f875`. The current player INI has head-relative movement and the aim
indicator enabled. The diagnostic INI already enables the scoped shadow shader.

## Changes and evidence

- `9f6d3bef` preserves raw XR stick axes when the selected Jackie avatar is in
  native traversal mode 6. The production input fixture reproduced forward
  becoming reverse at 180 degrees of headset yaw. After the change, both
  stick axes and the rest of the packet survive unchanged during traversal.
  A stale sample leaves the axes native while the protected owner contract is
  active. Stock executable SHA-256 `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
  Ladder mount `824028..82403A` calls `999010(6)`. The setter at `999010`
  writes the mode at `avatar+540`. Its first 49 bytes hash to
  `29C3C4C7B31894CC550FC5D4A024077193011B44AB383565F85269B196EBAB3B`.
  Ladder vtable `FFCF80+268` calls `7DC4B0` and stores the vertical traversal
  component at `action+6FC`. The update at `AC7CF8` uses its sign to choose
  direction and the dismount branch. No engine action, animation or executable
  bytes are patched. The reported after-Swifty dismount still needs gameplay.
- `7d75da2d` keeps the current frame's aim/head reference when recenter is
  requested after the eye matrices are frozen. The recorded click pose becomes
  active on the next tracked frame. The old implementation failed the exact
  production-body test. The new implementation passes the delayed handoff,
  moved-head and tracking/reference loss cases. The original shot-vs-laser
  report outside the recenter frame still needs actual gameplay captures.
- `25f7db01` applies the existing `SkipLightVs` diagnostic to both live and
  replay eyes for the known `68EEE79C` light volume. The live eye previously
  shifted its matrix with the diagnostic enabled. Default-off behavior is
  retained. This fixes the diagnostic's stereo inconsistency. The reported
  scene lighting and shadow artifacts still need matched gameplay captures.

## Validation and artifact

- `build.ps1 -DiagnosticCapture` passed. Candidate `d3d9.dll` SHA-256
  `CD4F68E75FCF41F46641CCAF07092DFACF0088358A1D7475CEFE5879DAF0F0B8`.
- `test/build_vr_settings.ps1 -OutputDir ./test-output/settings-20261128`
  passed all eight suites, including 65 actual host input checks, the recenter
  regression and 20,207 indicator checks.
- `test/build_protected_weapon.ps1` passed 130,923 production-source checks
  and 3,200 invocations of the pinned native fire body with fixture callees.
  The typed XInput neutral-loss fixture passed 2,423 checks.
- LitInstance mask/raster, clip-input particle projection and the actual
  shadow scope checks passed. The pinned `C4D1FB10` replacement compiled and
  ran on D3D9Ex. Selector 0 matched native pixels exactly and selector 1
  changed the expected 4,096 channels of the reconstruction-prefix fixture.
- Private `captures/simulator-20261128/run_probe.py` exercised the existing
  A76 x86 OpenXR simulator in a separate D3D11 fixture process. Both eyes,
  controller hover, one press and one release passed. The runtime DLL hash is
  `A76FC4855FA906B415B18D685733F85A753EDFD60B68789332861787C5B96A4E`.
  `probe-evidence/receipt.json` SHA-256
  `3F891005BC5D9ACF08A6FBBF48EC54A5076B5ACE5962D177D851BB90EA89B93E`.
  This is a runtime fixture result. It does not exercise Darkness II scenes.

## Open gameplay gates

- The leased baseline and `CD4F68E7` both reached the real crane-yard save.
  Simulator controller primary advanced the title. Left-stick selection and
  a separate right primary selected Continue from its visible red highlight.
  Pointer hover alone moved the visual beam, while the native Flash COM lease
  was refused. The title/Continue route used no Windows keyboard injection.
- Candidate PID `21448` accepted an L3+R3 click at frame `13402`, queued the
  displaced reference `(0.13, 1.7, 0)` at yaw 14.32 degrees, and kept stereo
  gameplay running. Native right and left fire consumer stages 32 and 33
  both reported applied, and both magazine counters decreased. Stick walking
  progressed past the stairs and into the crane yard. The save is before the
  reported after-Swifty ladder. Climbing and dismount still need an in-game run.
- The simulator's asymmetric preview frusta differed from the submitted world
  frusta by up to 0.244346 radians. Matching the simulator FOV to the submitted
  projection reduced that difference to 0.000000059 radians, holding the two
  beam poses and world submission unchanged. The visible left/right preview
  displacement largely disappeared. Same-frame native impact versus indicator
  and physical-headset alignment remain open.
- Baseline shadow frames `13681` and `16031` each exposed both C4D1FB10 draws
  per eye. The actual consuming c27..30 matched the respective successful
  stereo inverse upload and c223 was `(1,0,0,0)` in all eight draws. Numeric
  reconstruction residual peaked at 0.001634 game units across the sampled
  stationary and turned poses. The scenes had different native light/camera
  constants, so they do not support a visual temporal score.
- Both shadow frames staged all four source/final images but failed the exact
  layer-count check with the normal ammo HUD visible. Commit `3cac23cf` counts
  actual grip-space ammo quads alongside aim quads, while requiring one world
  layer and continuing to reject settings, wheels, gaze and cinema layers.
  The production-gate fixture failed with two ammo layers before the change
  and passes afterward. Protected-native build SHA-256
  `0987C95C744DEEA08E78C3980294DE798F69494BE233F080383D1D00575815E1`.
- Both sessions closed normally with exit code 0. Fresh save, settings, DLL
  and INI hashes were restored and the protected Steam/OpenXR state verified.
  Evidence under `captures/simulator-20261128/` remains local.
