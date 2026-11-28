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
- Two bounded attempts to launch the final capture build waited for the shared
  Steam lab and expired while other leases were active. The final capture gate
  has build and regression evidence. Its real-game capture completion and the
  affected lighting scenes remain open.

## Simulator follow-up

- Crane-yard PID `36108` ran the `0987C95C` candidate. With the simulator
  preview frusta matched to the submitted projection, both hands produced a
  new bullet impact on concrete in both eyes. Before/after head, grip centers,
  projection metadata and the corresponding beam poses agreed. The left-hand
  impact centers were 3.32 and 3.13 pixels from the fitted red line, and the
  right-hand centers were 2.33 and 3.41 pixels away in the 1600×856 preview.
  This measures the visible impact and line for two shots at this wall.
  `visual-candidate/aim-wall-audit.json` SHA-256
  `C6D7CBBD94C491E86FD3835B98C178BA708C83349C54F876C111FC811D19F639`.
- PID `38296` ran the separate opt-in diagnostic build `946D9722`, source
  `0d59d6a3` in the detached shot-diagnosis worktree. One admitted native
  right-hand shot at frame `20619` and the OpenXR pair shared display time.
  The mapped center ray matched the final native consumer's axis to the
  recorded precision, and the origin residual was 0.000000853 game units.
  Native dispersion shifted that shot by 0.407 degrees. The raw native
  camera and the selected render view matrices were inverse at recorded
  precision. The right-hand quad data matched the shot's tracked aim pose,
  with its log header missing. The wall-impact test gives independent visual
  evidence for both hands. `shot-shadow-witness/shot-witness.log` SHA-256
  `17BBD736112F85E141E93869F41FDEEF1A6B2CBDBE1F5C8FB74D48C6EF689BAC`.
  The witness fixture passed 698 checks.
- Capture submission diagnostics (`1f2adf5e`) exposed an additional failure.
  With shadow stages enabled, frames `18962` and `22040` produced all usual
  file stages but two cinema quads. Their logs show the world-activity timeout
  firing on the exact readback frames, then recovering. With pass stages off,
  frame `27907` validated through original Present with a single world layer
  and two ammo quads. The diagnostic can leave gameplay if readback takes over
  500 ms, even with a fresh native world camera.
- `aa21597b` accounts for the active recorder's elapsed time in the world
  heuristic when that same tracked frame still proves a world camera. The
  fixture covers a 900 ms readback, its next frame, startup dwell, loss of
  world observations and ordinary gaps. The x86 DiagnosticCapture build
  succeeded with SHA-256
  `36FBE77C4BC84F197EE962296798355D1E61EDA9A3D1BBFC33C55DA71380DBC5`.
- Both completed sessions exited normally and restored original game DLL,
  INIs, save, settings and protected Steam/OpenXR state. The crane encounter
  was reached from the user's save. The after-Swifty ladder and affected villa
  lighting are still missing from current simulator evidence.

## Full shadow-stage simulator captures

- PID `39456` ran `36FBE77C`. The neutral and turned poses produced complete
  shadow-stage captures `1401` / frame `42289` and `1402` / frame `42380`.
  Both had a single projection world layer, successful OpenXR and original
  D3D9 Present, and a verified readback provenance chain. Manifests are
  `CA4EDEC5F6B3CE1A3CDF27EE9BC3C34AC10ECFF4C21FA36AF81811CB8BA5A6F0`
  and `710BF92C70BA12208E453FFEDE09FFE76DFBE39415B8B2703B906FA59B3F3C8C`.
  The recorder's third attempt, `1403` / frame `44622`, returned to cinema
  during a capture at wider head yaw. The clock-difference fixture reproduced
  a failure in the first fix when the arm frame and previous submission
  shared a timer tick. Commit `7f97ce64` now uses integer milliseconds and
  clamps the excluded time to the observed gap. The fixture failed before and
  passes after. The x86 DiagnosticCapture build SHA-256 is
  `247FCC4C94167FC47215FAE4828DEF790998D65D66F9B6628AF40FEAB0F7D109`.
- `3775ce09` corrected the audits for the current capture format. The sampled
  depth audit uses the R32F sampler 0 input and the corresponding draw bank.
  The inverse-projection audit accepts the exact patched shader only after
  its SHA-256 is verified. The replacement was reconstructed from the pinned
  native shader and source constants, then compared byte for byte with the
  captured `4CC96C08` shader. Eight real consuming draws had effective inverse
  matrix error at most 0.00000144. Median reconstruction residual decreased
  from 0.1092 to 0.0021 and from 0.1026 to 0.0065 units across the two captures.
  There were 23,580 and 22,681 depth samples. These checks cover sampled
  reconstruction. The two native light projections differed even at the same
  body position, so the temporal test correctly refused to score the masks.
- PID `39456` exited normally. The receipt confirms the original Steam
  installation and protected player data were restored. Private evidence is
  under `captures/simulator-20261128/shadow-timing-fix/`. Validator suite passed
  42 tests. The archived villa capture lacks source images and cannot be
  rerun through the current validator.
- The A76 runtime exposes `grip` and `stick` through its low-level pointer
  command, even though the older `interact` client rejects those labels.
  Use an owned in-game capture and an explicitly released direct controller
  action. Private `crane_controls.py` contains guarded hold, stroke and release
  helpers prepared for testing. No propane throw or ladder dismount has been
  confirmed. The ladder fix remains scoped to the verified native traversal
  mode; reproducing the player report still requires the exact ladder.

## Integer-clock gameplay verification

- PID `4700` ran `247FCC4C` from the existing Steam checkpoint in the A76
  simulator. All four shadow-stage requests completed and validated through
  OpenXR and original D3D9 Present, including the wider head turn that failed
  on the first fix. The submission records show 812–5203 ms of capture time,
  with a single world projection on each request. Frame `8378` directly
  exercises arm and previous submission sharing a timer tick, with both the
  world gap and excluded capture time at 812 ms.
- The lighting audit observed sixteen `C4D1FB10` consuming draws with at most
  0.00000188 absolute inverse-projection matrix error. The two native light
  projections still changed even with body position and yaw held, so no visual
  shadow temporal score is available. Full retained stage files and receipts
  are in `captures/simulator-20261128/shadow-timing-ms/`.
- Continued through the crane arena and visually identified the darkling's red
  cylinder on the left ledge. A76 grip delivered multiple real native grab
  pulses. One attempt reached `NativeGrab busy` transiently. The cylinder
  remained visible on the ledge in both eyes, and no throw or damage to the
  crane was confirmed. An exposed route respawned at the original checkpoint.
  Compare the hand aim against the native prop-query camera before changing
  its mapping. The later ladder climb/dismount and villa highlights remain
  open. The simulator run closed normally with verified restoration of the
  original DLL, INIs, saves and protected Steam/OpenXR state.
