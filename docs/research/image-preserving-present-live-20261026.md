# Protected Present: first live test and explicitly authorized retry

Tested candidate: `7ed10a662c4a13b1631b7d7310f2707d6d96298f`,
`codex/image-preserving-present`. Result: **Present PASS; gameplay VR NO-GO**.
The user authorized the first session and then explicitly requested one retry.
No corrected candidate was launched. No merge.

## Exact artifact and installation protection

Both sessions used diagnostic DLL SHA-256
`ee0f3ca28ef29993ddda786b03e6a01a58973c88ae024f5488360095b628da24`
and the exact original package INIs. EXE SHA-256 remained
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
The existing read-only SHA/PE/instruction audit passed before each launch.
Only SteamVR/fpsVR were running; no other Steam game was interrupted.
Both launches were through Steam. Capture, native gameplay/input/FOV/sizing
hooks and the old engine-callsite patch were OFF; both protection opt-ins ON.

Each session backed up DLL plus all four game/local mod INIs before deployment.
After process exit, every original file and EXE hash was verified. Both rollback
receipts PASS; no candidate remains installed. Private logs, manifests and
rollback receipts remain under `%LOCALAPPDATA%/Darkness2VR/runtime/`:
`image-present-live-20261026-132416` and
`image-present-live-retry-20261026-133745`. No private full logs are committed.

## Runtime findings

- Session 1, PID37892/TID31116: last cumulative observer sample reports
  **44000 entries = forwarded = returned = D3D_OK**, recursion/errors/statuses/
  fault all zero. Normal detach reports44881 boundaries; Steam records exit0.
- Retry, PID38280/TID31976: last cumulative observer sample reports
  **33000 entries = forwarded = returned = D3D_OK**, recursion/errors/statuses/
  fault all zero. No native CRC-pointer exception recorded. After failed VR
  verification, normal UI close did not exit; MAIN terminated this exact owned
  test PID and restored files. Steam exit-1 is this termination, not a claimed
  spontaneous crash or successful normal shutdown.
- Both runs acquired the real additional swapchain from native return00964CC1;
  Present returned through00A94057 on a stable thread. Existing audited native
  dispatch proves this is the owner+20 Ex branch, supporting successful native
  QI9Ex. There is no separate per-QI runtime HRESULT receipt; do not invent one.
- Each run logged one successful startup Reset and reacquisition. An additional
  post-load Reset/recreation cycle was not forced or claimed tested.
- Guard was latched; acquisition was admitted by SHA/PE/original bytes; logs
  report EXE_WRITES=FORBIDDEN, no guard refusals/faults or legacy dispatch patch.
  This is the mod's image-write admission evidence, not a whole-process memory
  hash or proof about third-party software.
- Steam overlay attached and logged D3D9Ex CreateDeviceEx interception. Present
  continued without recursion with it loaded. Opening/closing its visible UI
  was not established by the attempted shortcut; full UI compatibility remains
  UNVERIFIED, rather than either PASS or a demonstrated incompatibility.
- Loaded gameplay was visible on desktop. Replay logs show eye0=1/eye1=1,
  overflow0, thousands of recorded commands per pair and matching frame/pair
  counters. Both XR swapchains/caches were2676x2728; actual source1920x1071.
  No resolution or quality improvement is claimed.
- Both runs logged an initial mode4 StereoSubmit/FIRST FRAME before the world
  heuristic activated. All periodic FrameContext samples had layers0
  (149 first run,111 retry). Steady gameplay cadence reported0 layers/s.
  Retry reached XR_SESSION_STATE_FOCUSED, sync0, tracking flags0xF while gameplay
  still submitted layers0. The user confirmed **black in PSVR2**. Inactive HMD
  alone therefore does not explain the failure. No xrEndFrame error was logged,
  but successful empty frames do not satisfy stereo presentation acceptance.

## Concrete rejection mechanism and bounded offline correction

The merged tested INI inherited `[Render] NativeStereoHeadGeometry=1`, while
its protected override set HeadGeometry=0. In `src/d3d9.cpp`:

1. UpdateHeadGeometry resets headDeltaValid=false and returns when HeadGeometry0.
2. BeginTrackedFrame treats NativeStereoHeadGeometry1 as headClipActive even
   with HeadGeometry0. In active gameplay it sets renderTrackingValid=false.
3. SubmitXR requires renderTrackingValid before admitting the generated pair.
   Therefore the protected profile generates eyes but intentionally sends no
   world layer. This is a deterministic configuration/source contradiction,
   supported by focused-session layers0 and the user's black-image report.

Offline correction adds only `NativeStereoHeadGeometry=0` to the protected
profile override. Tracking validation, renderer, DLL, FOV, quality, native image
protection and Present code are unchanged. It is **IMPLEMENTED / OFFLINE
VERIFIED**, not in-game verified or HMD accepted. The old tested private package
is preserved unchanged and must not be mistaken for this corrected profile.
`python test/test_image_preserving_profile.py -v`:2 PASS (including inherited
0/1 configurations and retained image/unsafe-hook restrictions); diff check PASS.
No DLL rebuild was necessary for the INI-only correction. No general framework
was added. The next authorized test must merge this corrected override into a
separately identified profile; never enable the old EXE hook to bypass failure.

Full protected VR and a separate PSVR2 acceptance package remain gated on actual
successful gameplay projection-layer submission and overlay UI verification.
Hands, targeting, tentacles, FOV, resolution and image quality remain out of scope.

## 2026-10-29: late Windows compatibility wrapper blocks acquisition

The deployed 48fa62e startup remained EXTERNAL_PENDING: CreateAdditionalSwapChain
returned D3D_OK through caller 6DA9109E (unqualified), while native Present caller
00A94057 was qualified but had no admitted acquisition. Read-only inspection of
the live device 0F7A58C0 found public slot13=6DA91050 in SYSTEM32/apphelp.dll;
the observer had installed the earlier D3D9 implementation 64B3CE30. Windows
therefore inserted its public wrapper after initial observation. A retained
successful run instead acquired from native caller 00964CC1.

After successful Reset/ResetEx, re-observe the receiver's current public Create
implementation. This uses the existing bounded external-entry admission and
module-pinning checks. It does not write a COM table, retain the receiver, probe
each frame, modify the EXE, or admit an inner wrapper return as native. Exact
native acquisition and Present checks remain mandatory; failed Reset does not
install another entry.

An actual x86 fixture inserts a wrapper after Start. Before the fix its first
native Present assertion failed, reproducing loss of outer acquisition. With
the fix, 1000 Reset/create/two-Present/release cycles pass: one forwarded create
per call, 2000 native Presents, 1000 accepted render pairs, unchanged COM tables
and EXE text. Existing base/Ex/private-slot, recursive-call, unqualified-create,
failed-Present, thread-change and main-EXE refusal cases also pass. Production
and DiagnosticCapture x86 builds pass. These are offline results; recovery of
this apphelp path and stereo output still require the next game launch.
