# Handoff: combined scenes, input, shadows and performance

**Date**: 2026-11-03
**Task ID**: combined-scenes-input-shadow-20261103
**Role**: MAIN
**Branch**: codex/combined-scenes-input-shadow-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: accepted source81bddf2b, DLL E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA
**Integration commit**: pending; no main merge or user candidate acceptance

## Goal
One combined candidate for native scene detection, shadows/reflections, input
conflicts and measured performance, tested in the process-local x86 simulator.
Resolution, accepted stereo, hands/tentacles, FOV and record ownership retained.

## Status
In progress. User explicitly renewed continuation after accidental Escape.

### Follow-up21:20 local
- Checkpoint57e7a455 committed earlier; current follow-up continues on the same
  dedicated branch. No final combined user candidate or main merge yet.
- Input poll now caches its repeated pause observation only within one
  SyncOpenXRGamepad call and exact manager/world key; no cached native pointers.
  Legacy tentacle gestures also yield to native action movies.
- Input fixtures203 production /257 diagnostic PASS; validator42 tests PASS.
  Production and diagnostic builds PASS (latest production preprocessor fix
  still needs a final diagnostic rebuild before freezing candidate).
- Two rejected diagnostic captures proved stateblock Apply re-entered constant
  setters and re-applied left-eye transforms. Capture-only exact-device raw
  restore plus explicit full float-bank restore fixes the diagnostic itself.
  New captures34008/931/9174 and34008/932/13691 pass strict validation, original
  PresentD3D_OK, VS/PS banks byte-identical before/after the INTZ copy.
- INTZ source170A0C80 is the actual depth attachment written by preceding
  native geometry. R32F readback is exact-size/hash checked and tied to source,
  consuming C4D1FB10 draw and explicit state-restoration witness. Old captures
  without that witness must not be used as validated converted-depth evidence.
- Actual-depth algebra at head yaw0/0.25: original median reconstruction error
  about0.117 game units; candidate about0.00026. This includes real viewport
  depth remap0.05..1; not an image/final shadow acceptance or FPS measurement.
- Test-only lab ShadowClipCoordinates=1 now enabled for visual A/B. Production
  default remains OFF. Current own game PID50972 start21:17:47; verify identity
  before closure. Original lab payload rollback remains unchanged.
- Video candidate now requires successful native draw, complete SHA-pinned
  YUV PS + clip VS, depth off/full viewport and SAME-FRAME final live RT identity.
  Resident Bink handle alone cannot select Video2D. No timeout/dimension guess.
  Simulator intro entered mode2 at frame1; title returned mode0 at frame2018.
  This fixes the sticky boot/movie classification witnessed here; actual villa
  TV comparison and final visual validation still pending.
- Diagnostic probe surface refs are released at a new frame and Reset/ResetEx.
  Reflection semantics, system-performance fix, special avatar recognition and
  actual QTE/execution/Darkling/carnival/ride scene tests still need completion.

## Changes
- Exact native movie-definition classification: QTE, execution, conversation,
  story. Retained native manager/movie/definition references, bounded names,
  pinned concrete tables/methods. Input protection only; visible asset presence
  is NOT proof of a currently open internal Scaleform popup.
- Native A/B/X/Y retained in protected contexts; wheel, double-X and gestures
  cannot consume those actions. Triangle first press immediately sends Y;
  second short press within350ms requests pause. QTE/execution/story protect
  rapid Y; ordinary conversation still permits the requested double-pause.
  First Y can already perform an action before pause is recognized.
- PresentMon analysis separates swapchains and warns about mixed aggregates.
- Fixed bounded elevated profiler broker: two captures, no game manipulation
  or generic privileged command channel. Its approved window has expired.
- Explicit diagnostic CapturePassStages=3 saves actual C4D1FB10 texture0 input
  and its surface identity for producer correlation. No routine readbacks.
- Explicit VideoDrawProbe=1 observes YUV decode draw targets and compares
  retained surfaces against actual final liveEnd RT0; observation only.

## Evidence
- Production input fixture176 PASS; diagnostic fixture230 PASS.
- PresentMon7 tests PASS; matched-capture validator34 tests PASS.
- Diagnostic build PASS. Latest production rebuild pending.
- Two loss-free ETW captures in private lab/perf. Both are villa/elevator,
  NOT combat. Collector phase labels do not establish gameplay activity.
- Capture2 native chain mean15.464/15.688ms; other chain33.301/33.337ms.
  Swapchain identity inferred, not physical headset FPS or A/B improvement.
  Game runningCPU56.524CPU-s over roughly55s ETL including merge; not proof
  of whole-machine100%. Running samples include driver/game/proxy work.
- Intro PS C1DC2D91 + VS7ACDFC64, depth off, rt != device backbuffer;
  actual final liveEnd RT0 == YUV target. Thus backbuffer-only admission fails.
  World-TV comparison still pending. No arbitrary dimension/timeout filter.
- Shadow depth encoding/producer and reflection semantics remain unproven;
  existing ShadowClipCoordinates candidate stays OFF. Do not report fixed.
- Lean-left/right action names found in QTE scripts; physical binding and
  special avatar types still need native proof. Do not bypass owner guards.

## Recovery / next steps
Private lab: test-output/combined-scenes-input-20261103. Its payload.json owns
exact original backups; tools/simulator-lab.ps1 Update retains them and Restore
checks game closure and protected file hashes. Latest game PID42568 at20:34:29
local, own emulator run. Recheck PID/path/start time before closing it.
Global OpenXR runtime remains SteamVR; simulator override is process-local.
No cache isolation or headset acceptance claimed. Do not add private traces,
shader bytecode, captures, saves, build outputs or local paths reports to public
history accidentally. Baseline remains unchanged.

1. Enter Continue, capture mode3 shadow input, validate Present witness and
   correlate texture/surface with its producer before any lighting patch.
2. Compare world TV YUV output with final liveEnd RT0; replace Bink-lifetime
   cinema classification only after presentation ownership is demonstrated.
3. Finish actual native contexts and input/runtime tests, production build,
   then freeze one candidate plus exact rollback. No new hardware claims.

## Follow-up after interrupted turn (2026-11-03)
- Continued own simulator process50972. Entered villa hall with visible NPC and
  static plant shadows; mode remained gameplay. Captures933/frame88431 and
  934/frame90424, head yaw0/0.25, both pass strict matched-frame validation.
  Converted depth bank restoration remains exact. Private fern-wall reprojection
  shows stable background shadow placement in this candidate pair; this is NOT
  an OFF/ON comparison or exhaustive shadow/reflection acceptance.
- Prior elevator OFF/ON reprojection residuals were essentially the same
  (mean0.002641 vs0.002692), a weak fixture without an active shadow caster.
  Do not use its algebra improvement as demonstrated visual improvement.
- Closed an ownership fallback hole: a selected RegionMgr local-player rejection
  now supersedes the optional renderer FOV override in the same renderer/frame,
  clearing stale Jackie aim/poses rather than re-admitting that older override.
  Synthetic ControlAvatar split-owner fixture proves foreign avatar rejection,
  valid old Jackie override, no foreign consume, balanced release including throw.
  This does not identify/support the Darkling/carnival native layouts.
- Cinematic fixture PASS; diagnostic build PASS after ownership change.
- CameraShakeTrace sampling/30-second gate now precedes native reads; expired
  diagnostics no longer keep sampling silently. No material FPS gain claimed.
- Intro presentation filter briefly returned gameplay for2 frames during an
  intro transition (484..486); no arbitrary timeout added. Check transition
  ownership before declaring all intro/video transitions complete.

## Continuation checkpoint (2026-11-03, 22:16 local)
- MAIN resumed after accidental interruption. Selected-player rejection now
  suppresses Jackie-only trigger remapping, wheel/equip pulses, crouch/gesture
  actions and objective-hold interception. Raw native controls remain available.
  Protected-gesture actual-body fixture:924 PASS; XInput production203 and
  diagnostic257 PASS; cinematic owner fixture PASS. Diagnostic and production
  builds PASS. This is safe native fallback, not proof of specific Darkling or
  carnival avatar layouts or their physical aiming.
- New diagnostic frozen at build/combined-scenes-input-20261103/
  diagnostic-special-input, DLL3632E212B263B7F258B74F6F06BF9F098C3638A4AB89ADB0524A8509B9C6E16C.
  Own simulator PID48508, start2026-11-03 22:06:56 local. ShadowClipCoordinates=1
  for bounded comparison only; lab receipt retains the original rollback.
  Root DLL may be a newer build: never assume it equals installed/frozen DLL.
- Villa walking and ordinary NPC view remain gameplay; selected owner accepted.
  OFF depth captures51720/937/76887 and938/79513; ON48508/939/34802 and940/37253
  all pass strict Present/depth/constant-restoration validation. Captures include
  visible NPC and floor shadow. Native body/animation differ across launches;
  no visual OFF/ON improvement or FPS gain established.
- tools/audit_shadow_clip.py --sample-depth now repeats the actual depth-grid
  calculation with validated producer/copy/restore evidence and viewport Z remap.
  OFF examples original median~0.119/0.120 game units vs candidate~0.000395/0.000596.
  Native inverse bias retained. This is algebraic evidence, not image acceptance.
- Added explicit single-frame capture of first C4D1FB10 consumer output after
  successful native draw. It uses existing bounded readback path without setting
  render state. Build/runtime validation pending; capture stalls are never FPS.
- Capture2 ETW now resolved with exact local DLL/PDB and a unique local symcache,
  without network symbol servers. Steady10..30s CPU weights: driver9.851CPU-s,
  game6.607CPU-s; proxy largest named leaves VS constants0.287, UI staging0.267,
  PS constants0.213, replay0.204, recorder lock0.191CPU-s. Samples are not
  recoverable savings. Current bridge fences~0.18ms and reader~0.006ms do not
  establish the cause of user25ms frames. No speculative worker/fence patch.
- Remaining: native special-context identity/ride classification, floor reflection
  producer, shadow output OFF/ON, runtime choice tests, consolidated candidate.
  Intro transition briefly leaves video mode for2 frames between closed/open
  handles; no arbitrary hysteresis added. World-TV validation remains pending.

## Resumed runtime checkpoint (2026-11-03, base Cinematic audit)
- Own simulator PID9284 (start22:19:12 local) is paused during a NEW test campaign.
  Before New Game, saved all four original villa/settings files with hashes in
  test-output/combined-scenes-input-20261103/pre-combat-save/receipt.json.
  MUST close owned game and restore these targets before leaving lab. Never
  leave the user's Continue replaced with the test campaign. Steam payload
  rollback is separately recorded in lab/payload.json.
- Installed diagnostic59F2A41B6FC80D6C17C022AEA2A064D7CAE9BC83FB08387DC8048B817542266E
  (3c682ffe); root DLL is a newer production build and is NOT installed.
  New frozen diagnostic-video-evidence CE8F1FB713307C20C4CD6E7048150ED408DAF27D87449BA7498F5A8D809B5C4E
  adds explicit presentedVideo transition evidence. Both build flavors pass;
  cinema, matched capture42, protected gestures924, input203/257 pass offline.
- Actual successful C4D1FB10 output captures941/942/943 fromPID9284 all pass
  strict producer/depth/restore/Present validation. Output is colored RGBA16F,
  not a pure shadow mask. Static villa wall/fern projection is stable for this
  pair; moving NPC animation and out-of-bounds reprojection are not defects.
  No complete floor reflection fix or demonstrated OFF/ON visual gain yet.
- Actual simulator double-triangle again opened native PAUZA during the
  restaurant sequence. Rapid native QTE/choice coverage remains untested.
  Initial look tutorial needs native stick LEFT to progress, not forward walk;
  absence of initial movement was scripted, not an input regression.
- Found omitted BASE Cinematic (vtableFCA7E0, metadata10ED3B8, native class
  registrationEAC3D0 stringF5BD18='Cinematic'). Restaurant scripted clips use
  it, including currentobject35F3B370. Derived D2 flags7BC/7BD MUST NOT be read
  from this base type. Native slot108/598C00 gates on byte2CC and drives a
  context camera route; state getter238/B7E570 tests708==2. Meaning and
  interactive-ride distinction still under investigation; no guessed patch.
- Native Lua InputControl wrapperDE8000 uses avatar virtual324/4E59C0, then
  virtual2F8/C7C710 returns avatar62C camera and copies camera+C smartref.
  Runtime this currently references the camera itself (owned cloned table),
  so it is NOT proof of a separate input-controller object or script identity.
  Do not derive Darkling/carnival type labels from that getter alone.
- Follow-up found the final-target video observer was compiled only in the
  diagnostic flavor. Extracted it into video_presentation.inl shared by both
  flavors, with all four successful draw routes and frame/reset cleanup.
  Presentation evidence now decides video mode independently of decoder-handle
  lifetime; raw Bink activity remains diagnostic context. Shader identity,
  full-target viewport and same-frame final-target equality are still required.
  No GPU readback/state mutations. Optional debug GetBackBuffer/FVF queries now
  run only with VideoDrawProbe enabled. Actual-body fake-COM production fixture
  passes failed-draw/replay/stale/world-TV-target/metadata/bounded ownership cases.
  Production build, diagnostic build, cinematic fixture, matched-capture42 PASS.
- Correction to preliminary base-Cinematic note: 598C00 is inherited by many
  entity types. Its2CC condition is NOT established as a camera-control flag.
  Do not use it in cinema classification. No base layout has been admitted yet.

## Corrected cinema contract and current test (2026-11-03)
- User explicitly authorized automatic computer shutdown AFTER the work is
  finished. Save/commit evidence and prepare candidate first; close owned game,
  restore user's original saves/settings and leave Steam out of simulator mode
  before shutdown. Do not force-close unrelated unsaved applications.
- ClosedPID9284 and restored all four pre-combat villa/settings files, hashes
  verified; receipt pre-combat-save/restored.json. New testPID46660 starts
  23:20:53 local, installed6BE0361A503FE2617A7E0D5C5B4404F2E23BFB3BDD1B4681BFFF6BE8DDB0F90E.
  This includes uncommitted corrected native cinema reader, not just7d143e67.
  Lab receipt source is older than source edits: hash identifies actual payload.
- Definitive correction: base Cinematic FCA7E0 serializer47FC00 contains the
  480140..4801A3 range. Actual descriptor names independently read inPID38076:
  CanDamagePlayer+7BC, FreeCam+7BD, OnRails+7BE. Old field labels were off by
  one, and admitted tablesFDD0B0/FDD318 were wrong. Native IsPlaying B7E570
  checks708==2. New reader pins exact type/getter/serializer and preserves
  gameplay for FreeCam/OnRails. See docs/research/cinema-control-flags-20261103.md.
- Exact-build read-only audit tools/native-camera/audit_cinematic_flags.py PASS;
  actual native reader/cinema fixture PASS. Diagnostic build PASS; admission
  cinematicContract=1 observed inPID46660. Production build still to recheck.
- Restored villa loaded through native Continue. No cinema during ordinary
  elevator/NPC view and walking. Two controlled right-hand pose snapshots move
  the visible right hand/running sleeve; protocol geometry confirms two distinct
  positions. Private fixed-cinema-hand-pose-0/1.png and hand-poses.json.
- MatchedFrameCapture/CommandHistory/VideoDrawProbe/CameraShakeTrace/CPUSyncTiming
  all0 for this run; no continuous readback collector. Simulator1 still lab-only.
- Main-menu PressStart ignores the first confirm when cancelling attract mode;
  two bounded confirms with release reach MainMenu.swf. This was native attract
  behavior, not a broken input binding. Main menu first item Continue works.
- Current game returned from pause to main menu for controlled new-campaign
  passive/interactive cinema transition testing. Original villa backup retained;
  restore closed-game again after New Game. No actual QTE scene pass yet.

## Runtime completion and recovery checkpoint
- Native restaurant PASS: passive Scene3D71398/76078/94560, gameplay return
  76055/78594/95169. Actual objects0F27F5C0 and2B2295C0 OnRails1 remained
  gameplay with visible hands during scripted walking. Presented movie101252
  selected Video2D; passive102074 and interactive103391 selected correctly.
  See cinema-control-flags research doc. Physical theater/HMD comfort NOT RUN.
- Production build PASS after corrected native flags. Final diagnostic rebuild
  33845A5F4D983E46D4BF589B2BD22AE6174C4435BF1DDCB81EDB2637D0D179A3
  from code5cd2de66; later commits only docs/package tooling. Runtime tested
  diagnostic6BE0361A... has the same code, independently rebuilt symbols are
  not interchangeable. Keep final DLL/PDB/MAP together in frozen package.
- Final cinematic fixture PASS, actual shared-video fixture PASS, matched
  capture42 PASS. Shadow driver bytecode fixture PASS outside sandbox with
  game closed. Earlier sandbox CreateDevice failure was environmental.
- Closed ownedPID46660 after exact path/start-time check. Runtime log retained
  privately as fixed-cinema-runtime-final.log. Test campaign checkpoint retained
  separately under restaurant-test-checkpoint; NOT the user's Continue.
- Restored four pre-combat files, then all four INITIAL pre-lab backups to
  recover original user settings as well as Continue. Initial Editor/settings
  differed from the later pre-combat backup; final identities recorded privately
  in original-user-save-restored.json. Continue0386D944..., EE C82548DD...,
  Editor4F23D3BC..., account settingsB163AFFF... . All target hashes verified.
- Restored exact accepted Steam DLL E56BF027... and diagnosticAC8290E4...;
  EXE/main render INI/loader protected hashes match. Simulator is no longer
  installed. No game left running. Global OpenXR runtime was never switched.
- New frozen-package tooling prepares ONE protected-native Combined candidate
  against this exact baseline, preserves main render INI/resolution and exposes
  optional ShadowExperiment separately. Default shadow OFF: mathematical depth
  evidence is not demonstrated visual improvement. All collectors/simulator OFF.
  Actual installer body passes isolated verify-only, both modes, exact restore,
  package/protected tamper rejection and foreign-preimage rejection.
- UNRESOLVED: user25ms/40FPS cause and recovery, floor reflection producer,
  demonstrated shadow OFF/ON improvement; actual QTE/execution/story and
  Darkling/carnival gameplay acceptance. Do not describe these as fixed, and
  do not merge this partial candidate to main or install it automatically.
- Frozen reviewable package: build/combined-scenes-final-20261103/Install.ps1,
  source1ff9395892e4b4c86acf7ca050ddb14426b72b62, diagnostic DLL33845A5F...
  with its own matching PDB/MAP. VerifyOnly PASS against restored Steam.
  Profiles differ only in named collector/shadow/simulator settings; main INI
  unchanged68EE5685... . Combined not installed; accepted E56BF027... remains.
  Explicit UNRESOLVED statuses are embedded in candidate.json/README.txt.
- User requested shutdown after work; after saving this checkpoint the session
  may request graceful shutdown without /f. A shutdown request is not proof the
  machine powered off (other applications may veto it). This does NOT mark the
  unresolved rendering/performance work complete. Resume from the package and
  evidence boundaries above, not by assuming all user symptoms were fixed.
- Continued offline AFTER package freeze: original historical combat ETL DOES
  retain CSwitch/ReadyThread despite empty arrays in old JSON exporter. New
  complete-frame parser/tests and combat-scheduler-waits-20261104.md record
  two actual long waits:38ms frame15.672ms transfer/NVIDIA dependency,31ms frame
  12.492ms native SteamNetworking availability poll. Readiness-to-RUN8/9us.
  Do not confuse the nearby SteamUser GetSteamID call with the actual long
  packet poll, or treat a signaling context as a lock owner. No risky Steam
  caching/network-thread/required-fence patch introduced. Historical old DLL;
  current user25ms/40FPS still unresolved. No new game launch or payload change.
- Further reflection audit found c8 eye/geometry inconsistency on exact
  VS12CCC8DB: native c8 vs raw-WVP center agrees <0.000414 game units, but actual
  per-eye center differs by median0.0325 in four validated two-eye captures.
  This is baseline/specular disparity evidence, NOT ownership of reported floor
  pixels. See reflection-eye-contract-20261104.md and audit_reflection_eye.py.
- New startup-only ReflectionEyeExperiment OFF by default, exact full-SHA VS/PS
  admission; draw-scoped c8.xyz correction with native/raw-bank identity gate,
  actual WVP and exact c8 restoration including failed-set attempt. Actual-scope
  fixture PASS, diagnostic build PASS, video observer PASS, capture42 PASS.
  No runtime/visual test yet. Frozen combined package remains unchanged.
- Reflection runtime follow-up: fresh isolated lab reflection-eye-runtime-20261104,
  DLL CCFFEB19..., source3f620c01; diagnostic and production builds PASS.
  Actual villa capture951/33677 has12 matching draws across two eyes, max c8 vs
  rendered-WVP-center error1.62e-6. Capture952/37484 after15cm head translation,
  yaw-0.5/pitch-0.15 has10 matching draws, max1.49e-6. Strict capture validation
  PASS. This proves this narrow correction reaches both eyes, not a broad floor
  or shadow visual fix. Actual native special contexts and HMD still NOT RUN.
- Closed own PID36772 after path/start check and unsuccessful polite close;
  restored exact Steam E56BF027/AC8290E4 and all four original user files with
  hash checks. No game left running. New package tooling exposes optional
  ReflectionExperiment, all experiments OFF in default Combined. Old frozen
  combined-scenes-final-20261103 package remains untouched.
- New consolidated frozen candidate build/combined-scenes-final-20261104,
  sourceb2065f0d, diagnostic CCFFEB197F85D0C9CE0C0C1D1E13B9EF9AD0EF016856EEC1454CBF2DC453A0EF,
  with its exact PDB/MAP. Same binary as the actual reflection runtime test.
  Default Combined, optional ShadowExperiment OR ReflectionExperiment; separate
  profiles, both experiments OFF by default, all collectors/simulator OFF.
  Exact Windows INI reads verify all three profiles. Actual installer fixture
  PASS three modes/rollback/tamper/foreign preimage. VerifyOnly against Steam PASS.
  No candidate install, main merge or push. Accepted Steam E56BF027 retained.
- Final recovery checks: four initial user save/settings SHA identities match,
  game absent, global runtime SteamVR steamxr_win64.json. Source tracked tree
  clean after final checkpoint; private build/test-output intentionally untracked.
  User-authorized graceful shutdown will be requested after saving this state,
  without /f; private shutdown receipt distinguishes schedule/request/result.
- Completion boundary: scene/control fixes and narrow reflection constant
  delivery tested as above. Current25ms/40FPS recovery and reported floor/shadow
  pixel acceptance remain UNRESOLVED; actual special-context gameplay/HMD NOT RUN.
  This is a testable partial candidate, never a claim that all user goals were
  completed. Resume these open investigations from saved evidence, not guessed
  matrix/Steam-network changes or another resolution reduction.

## Resume checkpoint: edge and hand shimmer, 2026-11-04

- User redirected focus to edges/hands if reflection motion is native. Viewpoint
  dependent reflection motion alone is not a defect; shadows are not exonerated.
- Sourceee9a6775 adds opt-in SMAA1x HIGH after NIS, defaultOFF, FXAAOFF,
  no resolution reduction. See docs/research/smaa-edges-20261104.md for exact
  source, GPU fixture and runtime boundaries. Cached six-draw D3D11 commands,
  no production readback/wait; actual hand-video crop shows fewer stair steps.
- Process-only simulator at3232x3300/eye PASS villa load, immersive gameplay,
  both hands present at two independent controller/head poses. HMD shimmer and
  clarity NOT RUN. No native outdoor CPU25ms/40FPS recovery demonstrated.
- Both builds PASS; hardware fixture1.260ms GPU BOTH eyes, CPU enqueue0.001ms.
  Actual installer four modes PASS; selected-draw8/matched-capture42 PASS.
- Own simulatorPID47916 exited. SteamE56BF027/AC8290E4, protected files and all
  four user save/settings preimages restored exactly. Global OpenXRSteamVR.
  No shutdown pending: the earlier shutdown note is historical, not an action
  to repeat while the performance/shadow scope remains unresolved.
- Freeze the next candidate at build/combined-scenes-smaa-20261104 after this
  documentation commit. ModeSmaaExperiment includes Combined scene/input fixes
  and onlySMAA, shadow/reflection experimentsOFF. DefaultCombined has SMAAOFF.
  Candidate is partial and requires HMD comparison; do not call all scope done.
