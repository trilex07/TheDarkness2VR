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

The shared lab lease was occupied at the first launch attempt. Do not use
another lease, change the installed player configuration or reload historical
saves. Use a fresh lease with the existing retained-close and verified rollback
lifecycle. Reproduce both-eye firing and compare the real shot and indicator
in the same frame. Reach the after-Swifty ladder, look across and behind it,
and verify ascent, descent and top dismount. Capture a fixed-head control and
translated/yawed pairs for the reported spotlights and shadows, including a
draw-level witness for any mismatched consumer. Headset acceptance remains open.
