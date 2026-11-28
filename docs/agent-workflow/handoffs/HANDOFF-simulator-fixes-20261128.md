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

## Same-pose control and steering comparison

- Experimental follow-up `06ca0db65f9c64e14e532e3d2a7472777b93ceb8`,
  diagnostic DLL SHA-256
  `D9D7343C621E12365D704A836F4927536794A308DF3ACF10CD2A33DEC79CFE51`.
  Built with `build.ps1 -DiagnosticCapture`. The existing native aim
  getter contract and 151,027-check ABI fixture passed again. This build
  enables mode changes only under the private diagnostic preset
  `grab-selector-comparison` and records candidates, pending acquisition
  and actual attachment separately. Both trial requests used PID `39612`
  and the same controller pose. The native camera matrix and reconstructed
  rays matched exactly in the logged precision at frames 9911 and 11407.
  Both used a 66.0-degree offset from stock camera forward.
- The two grips each reached the native LB handler. The barrel stayed on
  the ledge in both trials. With steering active, `+CF4` populated at
  frame 11531 with `4241ED80`, native vtable `00FFE800`. The pending and
  attached fields stayed empty. With no intervening translation input,
  stock pitch alignment let the game move one reference `01AA0490` from `+558` to
  `+580` with vtable `00FE1CF8`. The same entity was released by the
  tracked gesture and a cabin-area explosion was visible. After a death
  and checkpoint reload, a second stock-aligned cylinder reached
  attachment with the same vtable at a different position. The hold
  ended during the encounter before a second launch in that attempt.
- Keep the candidate steering experimental. The downstream `+CF4`
  branches around `00CC8F48` resolve a world position written to `+D00`.
  For actual attachment, the known left aim getter call at `00C6300F`
  builds two ray endpoints for `00BE1DC0` at `00C63060`. The same routine
  calls `00D3EE30` at `00C6314E` and `0099A2F0` at `00C631B8`.
  Follow the native request and acceptance path beyond the getter and the
  hover selector. `0090975C` assigns a retained candidate to `+558` and
  follows an entity-dependent native threshold at `009096D6`. The static
  call and field contracts are in the private `acquisition-live-writers-static.txt`.
- Private `grab-selector-comparison/comparison-native-proof.log` SHA-256
  `9F878FD669C8B63DCE2C08581380C19D24A04D9DF9A771A74A4BED6FF7A2BE9A`.
  The controller input and both serialized camera/ray results are in
  `same-pose-controller.json` and `comparison-pose-proof.json`. The
  positive hit capture `first-cabin-impact.jpg` SHA-256 is
  `16859208159DD8A6A44C276D57B9067A230498335B3D764CD7F640ACEA1550B1`.
  Both experiment sessions exited with code 0, verified restoration of
  15 protected paths and released the lab lease.
- To reach the ladder, use the normal encounter route and account for a
  second left-side cylinder before later spawns move to the right.
  The game spawns bandits after the first hit. Preserve survival using
  cover, crouch and available health restoration. The encounter and path
  past the diner to the ladder are documented in
  [Q and A, part 2](https://www.gamepressure.com/thedarknessii/q-and-a-p-2/z437d6).

## Private camera collision probe

- In the native left-fire update, `00CC7AC3` calls the camera's `+84`
  leaf. The leaf returns its Euler storage at `+110`. At `00CC7ACF`,
  `00CE2940` builds the forward row from those angles in degrees. The
  nested `00BF2750` call at `00CC7AE7` supplies ray position. The update
  builds and submits the separate collision ray at `00CC7B49` before the
  passive `+CF4` query. Its callback at `00C3A140` can retain a result in
  `+F24`. That reference is distinct from pending acquisition and
  attachment, and its role in attaching props is still unverified.
- Experimental source `e3e7d1b172c564c72fb7f2e39e22a71b6c0696a6`
  adds a private camera getter for the single Euler return address
  `00CC7AC5`. The position is admitted only through the matching BF2750
  call chain, same tracked frame and camera, after the Euler replacement
  succeeds. Before steering, the probe requires the reconstructed stock
  forward and the camera matrix forward to agree in the live call.
  The optional collision comparison keeps the passive selector stock.
  Source and flag changes remain on the experimental branch.
- `build.ps1 -DiagnosticCapture` succeeded. The exact Steam executable
  satisfied the static getter and collision call contract. The x86 fixture
  passed 168,066 checks including both leaves, opposite and vertical
  directions, private slots, native bytes, registers, FPU and SSE states.
  Pinned test DLL is private
  `captures/simulator-20261128/grab-collision-52F993AF39C8.dll`, SHA-256
  `52F993AF39C80F62ED7F9005CC88CF194BE0643A90360F143D23E2F3F21E2EEC`.
  The guarded launcher preset is `grab-collision-comparison`. It starts
  with steering disabled and can toggle it after reaching gameplay.
- Offline emulation executed the exact pinned native `00CE2940` routine
  for 475 yaw and pitch pairs. The maximum forward-component residual was
  `8.8112e-8`. Private `grab_native_angles_oracle.py` uses Unicorn 2.1.4.
  The result `grab-native-angles-proof.json` SHA-256 is
  `65B0D897886479CED95DE4ECB097C1CBB1E134B011142B2BAAB672432F2D48A3`.

## Isolated chapter selection fixture

- The mutually exclusive runtime lease was occupied when the collision
  probe launcher reached its wait limit. It did not launch or install the
  probe. The DLL remains pinned for the same-pose test.
- A completed PC save fixture is available privately from the
  [BESen save listing](https://savegame.pro/pc-the-darkness-ii-savegame/).
  Archive SHA-256
  `7FF7961832A98E268EDBA9010E74BDC10E3B119DA00596D51A7E16FB2D1B79AB`.
  The only extracted member is the 15,220-byte `CONTINUE.SAV`, SHA-256
  `E9826A420436CEA2663F7C56320D2B848F115E09ED190B80561077B8A93070E8`.
  The archive settings and shortcuts were excluded.
- `captures/simulator-20261128/chapter_session.py --prepare-only` passed
  with the main test DLL `247FCC4C`. The launcher sets `USERPROFILE`,
  `APPDATA` and `LOCALAPPDATA` only for its child. A separate Win32 call
  to the game's imported `SHGetFolderPathA` resolved both folders inside
  the private profile. The test fixture is under that profile's account
  directory. The real player save has never received the fixture.
- Launch the private chapter fixture with the same guarded lab lease,
  exact candidate digest, and existing restore workflow. Unlike the Steam
  URI, the direct child receives the isolated profile. The title PID must
  match the retained child handle before navigation. The Steam-authorized
  direct launch and the fixture's chapter menu remain to be established
  in the simulator. Use `chapter_nav.py` to inspect the title and native
  menu without automatically choosing Continue.
- The chapter launch also reached its seven-minute lease wait limit.
  The same external reservation was still active, and no chapter game
  process or session directory was created. The original
  `CONTINUE.SAV`, installed DLL and both INIs still matched the retained
  cleanup receipt. The offline protected-weapon fixture, both-eye
  shadow-scope fixture and all 42 matched-capture tests passed.

## Same-frame shadow follow-up

- The four retained crane frames expose eight pairs of actual shadow draws
  with identical native light constants across the live eye and replay eye.
  The before and after image hashes for the first draw agree in all four
  frames. Start with the second draw when measuring the crane.
- Diagnostic branch `codex/darkness2-shadow-pair-20261128`, commit
  `39d9611abe9161334c84b2036ad6917456b82464`, captures the same native
  shadow draw's before, depth and after targets in both eyes. Build SHA-256
  `2B9842C7650DF1680A83C3DFABCC35F70F177863F5C37ACC88718A777D49345B`.
  The 47 capture tests, both-eye scope fixture and isolated chapter
  launch preflight passed. The separate branch handoff documents mode 5,
  `CaptureDrawOrdinal` and the reprojection audit.
- The private `world.py` helper accepts `shadow_capture(serial, 5, 1)`
  inside the exact owned gameplay session. Use the paired DLL in
  `C:/Users/Trilex/Darkness2VR-shadow-pair-20261128/d3d9.dll` for the
  isolated chapter trial. The lab was still reserved, and the first live
  ladder and Villa captures have not run.
- The three-minute paired-candidate launch wait expired before acquiring
  the lab. It did not create a chapter session directory. The protected
  Steam DLL, both Steam INIs and player save matched their prior receipt.
- The shared-color regression test still matched hard-coded slot zero and
  one after slot rotation was added. The updated test verifies the selected
  slots and both freshness checks. All top-level Python suites passed,
  93 on the task branch and 98 on the paired diagnostic branch.

## Native ladder state-machine fixture

- `test/native_ladder_contract.py` runs the exact Steam x86 instructions
  for the mode-6 mount, movement input, ladder update and animation event
  handler. The fixture supplies a synthetic actor, animation services and
  rung-end events. It verifies neutral input, a downward rung and bottom
  animation 14, four upward rungs and top animation 8. At the same upper
  rung a reversed packet queues downward animation 12 instead. The native
  `LadderDismountTopEnd` handler targets the mounted actor, enters
  state 4 and the next update returns finished.
- The eight host suites also passed again, including 65 checks of the
  actual XR input route. At 180 degrees of head yaw a positive stick becomes
  negative during ordinary walking and stays positive in traversal mode 6.
  This gives a mechanical explanation for reversal on the ladder. The
  fixture does not establish collision, camera behavior or the real scene's
  animation timing at the top.
- Reproduce with Python, pefile and Unicorn 2.1.4 using
  `python -B test/native_ladder_contract.py "<Steam DarknessII.exe>"`.
  Private `captures/simulator-20261128/native-ladder-contract-proof.json`
  SHA-256 `94691D26A0E410210AC9AAE23FA3B0D010F534851403F2F41DF27C33FA64EE3A`.
  The game executable was opened read-only. The shared reservation still
  precluded an owned simulator session.

## Composite fog reconstruction

- Retained crane frames 8195, 8286, 8378 and 8474 have the same native
  camera projection and view-projection. Both eyes in all four frames
  exercised PS `33DE98C1` with effective inverse-projection constants at
  c10–13. The captured DOF-only shader `A69BDC80` still multiplies both
  screen clip coordinates by sampler 2 depth before reconstructing height.
  Its recorded vertex shader `24E3903D` exports those coordinates directly
  from clip X and Y. `tools/audit_composite_fog.py` verifies both original
  captured shader digests and the relevant disassembled instructions.
- Across 9,327 synthetic geometry points visible at all eight recorded eye
  poses, the maximum reconstructed height spread was 0.02605 game units.
  Unscaled X and Y reduced it to 0.00850, and also normalizing the world
  viewport's depth range reduced it to 0.000203. This controls the native
  camera and measures the recorded shader arithmetic. Exact sampler 2
  depth pixels and the reported fog region still need an in-game capture.
  No rendering code changed.
- Run `python -B tools/audit_composite_fog.py <matched frame directories>`.
  Private `captures/simulator-20261128/composite-fog-height-proof.json`
  SHA-256 `FABC61CA08CEA93C4EF7B7B20A4A78C3B8F2A06145B4AD39D5FDE7E1F9CDFEF7`.
- The paired chapter launch passed its isolated-profile preflight and
  expired after six minutes waiting for the same external lab reservation.
  The private navigation helper `capture_shadow_pair(serial, ordinal)`
  now verifies the game PID, capture serial, frame, build and temp path
  before running the audit. Its offline checks covered successful evidence,
  duplicate serials, a foreign build and a rejected frame.
- Diagnostic branch `codex/darkness2-composite-depth-20261128`, commit
  `276c757cbd555becbaf883b4a815e528474e0147`, adds mode 6 to copy the
  composite's actual sampler 2 INTZ depth in both eyes. It records each
  producer, sampler state, constant-bank restoration and successful
  consumer return. The Win32 build, 101 Python tests, four real legacy
  captures and isolated chapter preflight passed. Candidate DLL SHA-256
  `A7C28563FD1143920A55B8500C83EDD69199734E9772B314E12EF106722B890B`.
  The private chapter helper accepts this DLL for both mode 5 and mode 6.
  The exact source format and rendered result in the target scene still
  require gameplay.

## Gameplay after M12 released the lab

- The archived chapter fixture launched in the private Windows profile.
  The native title presented New Game without Continue, and the imported
  save was removed inside that private profile. A separate isolated
  profile with a byte-for-byte copy of the real Steam checkpoint offered
  Continue and reached the crane yard. The player's original save was
  protected in both runs.
- PID 36756 used the grab-collision candidate for a fixed-position
  comparison against the left ledge cylinder. The stock camera's forward
  was `1,0,0` and the tracked ray was
  `0.507305,0.6085227,0.6101982`, a 59.5-degree offset. The stock and
  steered off-axis grips produced no pending or held cylinder. The
  steered native collision path reported several +F24 candidates, while
  the cylinder stayed on the ledge. After native body-camera alignment,
  two `00FE1CF8` cylinder objects advanced from +558 to +580. Tracked
  throws cleared both attached pointers. The encounter later reset to
  the crane checkpoint. Evidence is under `grab-collision-comparison`,
  including `live-grab-proof.log`, `control-settled.jpg`,
  `steer-settled.jpg` and `native-positive-2-held.jpg`.
- Both probe draws ran in each eye in frame 3990, but the first paired
  fog capture and paired shadow frame 10759 invalidated right-eye replay.
  The one-eye request 13634 succeeded. Exact-device scratch snapshot
  restoration incorrectly went through the render-target hook's
  external recorder mutation guard during replay.
- `5c8e2179b7fa0e4693ac8c2fba263204bf13149e` forwards only the paired
  capture's scoped restore through raw color and depth setters. The
  narrowed build `F7DD2B35` completed the actual mode-6 fog depth capture
  at frame 7133 and mode-5 shadow capture at 7251 in the isolated
  Steam checkpoint. The shadow audit matched 21,710 points across both
  eyes, with median mask error 0.00155 and p99 0.0182. Both renderer
  captures used the crane yard, and the Villa still needs separate
  testing. The branch handoff documents the build and regression tests.
- Actual composite depth samples from the earlier repaired captures at
  frames 2990 and 4906 support the world viewport range `[0.05,1]`.
  Applying that range reduced median reprojection residual from
  `2.20e-4` to `1.08e-5` and yielded 31,056 depth-consistent points.
  These frames also changed the native camera. The sample receipt is
  `composite-depth-replay/live-fog-viewport-probe.json`.
- An empty private campaign with `F7DD2B35` exposed a menu regression.
  Stock A worked through the main menu but could not leave the difficulty
  screen. Native Flash cursor admission refused the observed Steam COM
  method, yet the code still forced the movies into mouse mode. The
  fallback in `2eb3b74771de70363d1ebbd1a13068804003aa55` preserves stock
  controls on refusal. With `E23CDE3B` the same stock A inputs reached
  brightness, the recap choice, and Dinner Out. The branch's native Flash
  fixture passed 51,218 checks. Find the screenshots and extracted logs in
  `chapter-new-story` and `chapter-new-story-menu-fallback`. The later
  private campaign created a native Continue checkpoint, which passed
  process-local profile preflight for a future resume.
- A separate stock-body-camera trial in `chapter-player-stock-look`
  used the locally accepted Steam checkpoint at Gangster difficulty and
  confirmed two real throws with captured crane sparks. After the second
  throw the game loaded the same checkpoint. Scouting the right approach
  after reload identified cover and a native pickup prompt, but did not
  establish the third cylinder or ladder. The closed private launches
  each restored the seven preexisting Steam/save paths and released the lab.
- The same archive with its matched 3,512-byte settings file also failed
  admission. Native startup removed both files inside the private profile
  and the menu had no Continue. A fresh local campaign did produce an
  accepted Continue, but a stock resume repeated the restaurant checkpoint.
- The fallback build also let the simulator change difficulty in the copied
  Crane profile from Gangster to Oprych. The native keyboard and tracked
  throw trials could still lose the cylinder or restart the encounter.
  The stock settings and checkpoint were restored on closure.
- A private fixture on local branch `codex/darkness2-private-route-fixture-20261128`
  and commit `b9084185eba1eb583edbaa3f5638f15d6301876b` uses the game's
  admitted current/max health and setter to keep the disposable campaign
  alive while navigating to target scenes. It requires the process-local
  simulator and an owned marker file. The stage will be disabled before
  testing ladder or Villa behavior. DLL SHA-256
  `4C945F856CC88A8B4F10F89D196A52DD3A6BDDF39940132B4ACD6E3F1BE00DEA`.
  The crane run produced 444 recovery receipts, confirmed two native
  releases, and traversed the right lane. Its checkpoint remained at the
  crane, so those releases do not prove the encounter was won. The marker
  was disabled before the guarded close. Evidence is in
  `chapter-private-crane-route/route-runtime-proof.log`.
- The same fixture advanced a separate Dinner Out profile through the
  first firefight and into the van checkpoint. The new private save had
  SHA-256 `661B083FED859E7F5D237879BB372A1F21D62DB2A1F4ED0AF58A46C0C1F1A142`.
  The later bar firefight did not reach Payback during this run. The
  fixture was disabled and the owned game closed before preparing an
  independent stock-weapon comparison from that private save.

## Direct chapter loads and manual Steam handoff

- A verified disposable Steam copy at `D:/Darkness2VR-chapter-lab-20261128`
  used the normal retail map loader. The private menu fixture redirects only
  the New Game callbacks to the game's existing map list after registering
  the normal menu handlers. The exact Lua 5.1 closure capture words at byte
  offsets `54474` and `54522` changed from `02000000` to `02800000`, binding
  local slot 5 instead of 4. Both base and `PCPatchManifests` script archives
  need the change. The menu script SHA-256 is
  `5792767A9FB7C29EE8DF18438BE703B52D4F386CFB6B0307CFE421DFE489682F`.
  The private receipt and reproduction script are
  `private-level-script-archive-new-game.json` and `stage_map_selector_new_game.py`
  in the simulator evidence directory. Native chapter assets and the Steam
  installation stayed stock.
- PID 34444 loaded `PokerWarehouse.level` with candidate `E23CDE3B`.
  Jackie followed the darkling into the warehouse alley until a chain-link
  gate blocked the way. The validated read-only native avatar witness still
  reported mode 0 there. A grip request and a jump did not pass the gate.
  The warehouse asset identifies two native grabbable gates and named ladder
  actions. The actual ladder mount, ascent and roof dismount remain open.
  `chapter-private-map-warehouse/warehouse-selected.jpg` has SHA-256
  `2B9E97E722D115875753ED0171F44D35095A07B685ACA75CEA91DB6D9FC7B2A8`.
  The run restored the staged DLL and released the lease.
- PID 28720 loaded `PokerHub.level` at the Villa elevator with `E23CDE3B`.
  The matched Villa fog-depth frame 10015 and paired shadow frame 10118
  both passed capture validation. The shadow mask audit rejected frame
  10118 because the selected draw supplied insufficient shadow contribution.
  A second shadow ordinal requested from the lobby did not produce a complete
  pair at frame 13298. The visible lobby capture and original capture files
  are in `chapter-private-map-villa`, with before/after SHA-256 verification
  in `retained-captures.json`. The lobby screenshot SHA-256 is
  `F6A8AF5A474E00675EF43D7B3BCB1EB4EFD8CD5888EDC060DB465212027B8B71`.
  The Villa rendering reports remain open. The stock install and player
  save matched the pre-test hashes and the simulator lease was released.
- At the user's request Steam started the installed build with the player's
  real save available under Continue. The verified pre-play save is backed up
  in `manual-steam-handoff-20261128`. Desktop presses did not activate Continue.
  SteamVR reported an unfocused session and no active controller profiles.
  The user takes over from the visible main menu. The manual play lease stays
  active until the owned game exits normally.

## Standard tentacle acquisition branch

- The entry at `00909310` receives the fire object in ECX and the candidate
  reference in argument two. `00909329` checks the candidate class descriptor
  `010FB4B0`. The direct route writes the retained reference to `fire+558` at
  `00909381`. The standard route obtains its interface from argument one via
  `008FDDA0` and invokes vtable slot `+168` at `009096D4`.
- Executing the native x86 bytes offline confirmed that `009096E0` rejects
  when the interface result exceeds the value at `fire+724`, or when the
  comparison is unordered. Equality continues. Both `00909381` and the native
  retained-reference setter called at `0090975C` change the pending field.
  Neither of those stores advances the held field at `+580`. The offline
  proof JSON SHA-256 is
  `1406557268C1F931E8963B4A35F32896CEBFC9441200F2151B85ADD0B707EB97`.
- The private `map-grab` launcher passed preflight with the verified pre-play
  crane checkpoint and its exact staged settings. The x86
  `grab_acquisition_witness.exe` built at SHA-256
  `B5734C0FBF9C3721123603BF71B87138478E81BDAB25BD92D93DB59DEF0736BD`.
  It admits only the disposable executable, exact process creation stamp and
  four pinned instruction sequences. An unrelated child was refused and
  remained running. The observer is bounded and restores the thread registers
  it acquired before detaching. No debugger was attached to the player's run.
- Once the lab is free, open `map-grab` with the admitted comparison DLL
  `52F993AF`, then use `grab_session_nav.py` and `grab_acquisition_window.py`
  in the owned crane scene. Collect the entry caller, candidate vtable, limit
  branch, pending and held fields for the matched off-axis and stock-aligned
  grips. Keep candidate selection and actual attachment separate when deciding
  where the fix belongs. The live failure is still open.
