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
- The next diagnostic is separately built in
  `C:/Users/Trilex/Darkness2VR-grab-diagnosis-20261128`, detached commit
  `d8372669`, DLL SHA-256
  `4271BE0B23B710B2A8B4A4088072CAFF8EBD8DC1F3FAE65610E787D37607C2F2`.
  `LeftGrabRayWitness=1` logs the exact native getter input/result and any
  same-frame native render view/projection submission during left squeeze.
  It preserves target selection, movement and gameplay. The private
  `session.py left-grab-witness` preset stages only that flag on the existing
  profile. The x86 build passed.

## Native left-grab witness at Swifty's crane

- PID `36128` ran the separate `4271BE0B` diagnostic. Followed the shaded left
  side of the arena, tested seven distinct A76 left-grip presses across
  several viewpoints, and saw seven stock grab pulses. One grab reached a
  real native held entity, `44C47290`, first observed on frame `8378`. Both
  eyes show a metal rod on the tentacle and the red cylinder still on its
  ledge. The six later presses did not reach a confirmed attachment. No
  cylinder throw, stock throw pulse, or crane damage was observed.
- The first probe's simulated ray and same-frame native getter output agree
  to within 0.000003° in all 90 comparable samples after converting XR
  handedness and applying the native camera basis. At the first observed
  attachment the getter origin and the native render VP camera center differ
  by 0.00000166 native units. The largest origin difference, 0.334 units,
  occurred after the hold was empty. This measures the getter and render
  camera at their recorded sites. It does not identify the native query's
  candidate ranking or establish the hull and visibility of the cylinder.
  No production aiming change was made from this probe.
- Private evidence under `captures/simulator-20261128/left-grab-witness/`:
  `left-grab-witness.log` SHA-256
  `3E560255920B22417CDEF66E84C70BEF05CC14AD6C0E61B1F393F7C3B2AEDD8A`,
  `probe-a-held.jpg` SHA-256
  `1CFC841E9B7BD1BDA360758C6A8226BF4BB39985F3B888DB239C113CC0D92409`,
  `probe-a-ray.json`, and `analysis.json`. The adjacent private
  `analyze_left_grab_witness.py` recomputes the measurements. The session
  closed normally with exit code zero, all 15 backed-up paths verified,
  Steam/OpenXR state verified, and the lab lease released.
- The first witness budget was consumed during the first hold. Future
  acquisition diagnostics should budget by press and compare with the
  same checkpoint using native gamepad look and the stock grab ray.

## Native-look control trial and recovery

- The first control profile allowed native right-stick pitch and set
  `ProtectedLeftAim=0`. The mod refused that configuration in DLL process
  attach because the armed protected throw and tracked hands require the
  protected aim feature. The title window did not appear and no simulator
  input ran. Exact rejection log SHA-256
  `5B1585DF3527705350EF58DBFB9218287F7EC432CFC10D61D169B93F265D4626`.
- The automatic close path retained the lease because there was no title
  window or retained process handle. The MAIN recovery verified three
  independent process inventories and the exact current lease before
  working from this session's fresh receipt. Verified all 15 original
  paths, including the unchanged save, original DLL and both game INIs,
  confirmed unchanged Steam/OpenXR state, and released the lease. Private
  receipts: `stock-look-crane/manual-cleanup-verified.json` and
  `stock-look-crane/manual-release-status.json`.
- The revised private preset `stock-look-aligned-crane` keeps protected aim,
  throw, and hands enabled and only permits stock right-stick pitch. With
  the headset and hand aiming along the neutral reference, the private
  witness can check that acquisition consumes the same origin and forward
  direction as the native camera. This control awaits an available shared
  simulator slot. There is no reason to relax the native startup guard.
- Checked historical named Continue backups and backup manifests from the
  retained worktrees. The available copies have the same SHA-256 as the
  current pre-crane checkpoint. No separate ladder or Villa save was found.

## Cylinder acquisition and simulator motion

- PID `37476` reached Swifty's crane using the corrected native-look control.
  Native right-stick yaw and pitch put the illuminated cylinder near the
  center of both eyes. The second sustained grip successfully attached the
  red cylinder and lifted it off the ledge. The held-cylinder image
  `stock-look-aligned-crane/native-centered-settled.jpg` has SHA-256
  `311AD5FC9AF53EAC151BBC726488D6AB1A6DCB6ADBD057F84705ABF2E2C02CE1`.
  With A76, stepping the controller pose through six acknowledged updates
  and releasing the grip left the cylinder visibly attached. There was no
  stock throw pulse. This separates acquisition from the failed throw.
- The pinned A76 simulator samples controller velocity inside desktop input
  polling. `SIMXR_AUTOMATION_ONLY=1` skips that block. The same runtime
  advertises valid hand velocity to `xrLocateSpace`, so the mod takes the
  reported zero instead of differentiating the located positions. The
  original file-driven steps were also 7 to 11 frames apart, too sparse for
  the recognizer's 35 ms coherent-motion window. Both conditions precede the
  native throw path. No production grab or throw behavior was changed.
- Simulator branch `codex/darkness2-simulator-motion-20261128`, source
  `4609138`, isolated worktree `C:/Users/Trilex/Darkness2VR-sim-motion-20261128`.
  The simulator now samples automation velocity after applying the current
  pose and accepts a bounded grip stroke over successive frames followed by
  a release at the endpoint. Win32 build and pointer lifecycle tests passed.
  The process-local simulator DLL SHA-256 is
  `99383EBA0224747C3E4D151A1F428E752B640ED4FDD0D0DFECFBCAD072360108`.
  The existing A76 runtime remains frozen.
- The close of PID `37476` restored the original DLL and all protected paths
  and released the lease. Two further live shadow-stage captures from the
  west wall, frames `22925` and `24185`, passed matched-files validation.
  Their native light or shadow projections differ, so the temporal mask
  audit rejected the comparison. This position does not provide a static
  light for scoring visible shadow behavior.

## Powered simulator throws at the live encounter

- PID `36540` used the process-local motion runtime with the same
  `4271BE0B` diagnostic. Stock camera alignment acquired two full-size
  cylinders, both visible attached to the tentacle before throwing. Two
  frame-paced gestures produced stock throw pulses and real releases. The
  second emitted `NativeThrow APPLIED` for entity `427086A0`; the engine
  supplied speed 76.704 and the mod's redirected velocity retained that
  magnitude within the rounding of the logged components. Both throws
  produced visible explosions. Crane completion and the ladder remained
  beyond this session.
- A hand-aim-only attempt earlier in the same run reached the getter with
  local direction `(0.107587, 0.056418, 0.992594)` while the cylinder stayed
  on its ledge. In the 94 complete witness frames the getter and selected
  native render camera forward bases agreed, with camera origin within
  0.000005 units. None of those probe frames showed an attached entity.
  This does not isolate occlusion from candidate selection, and the route
  moved between failed and successful requests. For a causal comparison,
  hold the actor at one verified location and observe the acquisition
  callback's result for both controller-only and stock-aligned inputs.
- Private `stock-motion-crane/motion-native-proof.log` SHA-256
  `83DE632F4A959D7308D09088689A00634967E5BAB020791BDB8F3494308F375E`.
  `motion-throw-3-result.jpg` SHA-256
  `8A288277EA82E04AF4E67D9367B15B96254F65561978C48F223B418802010555`.
  The action receipt records a completed 12-frame gesture and observed
  grip release. The simulator refused the earlier expired hold and the
  next action deliberately re-armed the grip while the prop stayed attached.
  The game exited normally. The retained-close receipt confirms verified
  restoration of 15 paths and the shared Steam/OpenXR state, and lease release.

## Scoped grab candidate experiment

- Experimental source `034edc92bb704bf03d719491638993d14d8d3572`
  in `C:\Users\Trilex\Darkness2VR-grab-selector-20261128`, branch
  `codex/darkness2-grab-selector-experiment-20261128`. Diagnostic build
  SHA-256 `3705542199D131D6382C3D7CAB2F10043E463E91E024252D3EE70F3ACBEA70E4`.
  The x86 getter ABI fixture passed 151,027 checks, including exact frame
  ancestry and rejected return chains. The native getter and throw-selector
  static contracts passed against the pinned Steam EXE.
- The left-fire update calls the shared target selector at `00CC8B10`
  (return `00CC8B15`) and stores a candidate reference at `+CF4`.
  The experiment substitutes the tracked private camera transform only in
  that caller's direct getter and two helper calls. It distinguishes the
  release selector's separate caller and avoids executable-image writes.
  Startup flags are `LeftGrabSelectorWitness=1` and `LeftGrabSelectorSteer=1` in
  the staged diagnostic profile. The private launch preset is
  `grab-selector-steer` in `captures/simulator-20261128/session.py`.
- PID `28024` admitted the exact native getter. At frame 19236 the controller
  direction was 57.4 degrees from the stock view (stock forward `(1,0,0)`),
  the game entered the busy state, and both eyes showed attached rebar.
  The native throw receipt for the rebar identifies entity `0195E9F0`.
  A separate grip while pointing at the cylinder from the same side of the
  yard kept an untyped candidate at `238A4280` without attaching anything.
  A later grip from the wall with the stock view closer to the target
  acquired a cylinder. The controller gesture redirected and released
  entity `019626C0` at native speed 62.658 and the capture showed an explosion
  by the cabin. The player died and the pre-fight checkpoint reloaded.
  No same-position experiment-off result or cylinder-only steering proof
  exists. The crane completion, ladder and Villa checks remain open.
- Keep `+CF4` distinct from the `+558` and `+580` fields. The
  `00D3A9F0` writer at `00D3AC4B` consumes `+D30` and `+D38`, whose
  native metadata names are `mDemonicShieldType` and
  `mDemonicShieldUpgradedType`. Follow the standard prop path instead,
  starting at the `00AC0A90` caller near `00909759` and the live native
  grab update. The next comparison needs two grip requests with the same
  actor position, stock camera and controller ray, along with typed candidate
  and actual attachment observations.
- The private `grab-selector-steer/selector-native-proof.log` SHA-256 is
  `13335CADA4982A32670E84DEFCD1D4243A472246A8CB1F2E848BABC0FE863604`.
  The representative frames are `steer-held-2.jpg` SHA-256
  `2B5EBB0EAA9D3095F93E3863131184A9C505C4E27971711D46366CC040262EAE` and
  `wall-fine-held.jpg` SHA-256
  `BCB7CE5B61F2563FA94198C088138792817282035389CC08AFB41AB0775F1767`.
  `cabin-result.jpg` SHA-256 is
  `A6A86F4ABAD9861D955D6F5F264F3361E2F2D1A62EA1CE159837E5173695E706`.
  The same folder retains the ordered route captures and input receipts for
  a control rerun. The private navigation guard used the native pause
  transition, world activity and weapon publication because the movie flag
  persisted after Continue. The owner and foreground guards still applied.
  The game exited normally, all 15 protected paths passed post-restore
  verification, the original DLL and both INIs matched, and the lease
  was released.
