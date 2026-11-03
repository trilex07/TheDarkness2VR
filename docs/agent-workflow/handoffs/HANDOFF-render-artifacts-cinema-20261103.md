# Handoff: head-relative lighting, hand flicker and false cinema

Date: 2026-11-03
Task ID: render-artifacts-cinema-20261103
Role: MAIN
Branch: codex/render-artifacts-cinema-20261103
Worktree: C:/Users/Trilex/Darkness2VR/.main-camera-shake
Baseline source:81bddf2b4ec734a8fab5d885e01099b17752606e
Integration: pending; status: In Progress, no runtime fix claimed.

## Current installed state

User identified last good FPS as Combined+IsolatedEyeOutput. Closed-game frozen
rollback chain source-resolution package then gaze-FXAA package completed.
Independently verified DLL E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA
and INI68EE5685C7AE9FEC8F1AE2677D551DD90C7BCC21DD66737560815EEF4AB26EA9.
All four isolated-eye candidate protected targets match. Game not launched.
Accepted resolution2676x2728 restored. Local source still descends from rejected
source-resolution/FXAA branches; DO NOT deploy the working-directory DLL or
assume source HEAD matches installed payload. Keep accepted rendering invariants.

## Evidence loaded

BASELINE, workflow, failed approaches and villa/HUD lighting task handoffs read.
User MP4 Steam35840D~1.MP4:12.010667s,3840x1080,60fps. Private extracted frames
and original log under test-output/source-resolution-rejected-20261103.
Floor highlight swims during head motion. User also reports shadows; no shadow
video/save available. Video is a desktop mirror, not a matched per-eye draw dump.
Do not label specular motion alone proof of a projection defect or shader ID.
Hand12fps contact sheet does not prove whole-hand alternating disappearance.

Runtime trial log: RT0 actually3200x3264, isolated output active15000/fallback0;
late world-bearing cadence roughly46/s. PS/NIS timestamp spans are not GPU Busy.
Original Present and wait/fence timing cannot attribute whole CPU25ms frame.

## Cinema diagnosis

False villa transitions are mode2/native0/video1, including frame9192 and13269.
L3+R3 temporary overrides return gameplay while video stays1. Native cinematic
FreeCam fix is not the failing path. BinkWait/Close registry proves codec lifetime,
not fullscreen presentation. Preserve manual60second override and natural auto.

Added read-only CinemaOwnershipEvidence.java; executed against existing Portal
Ghidra project with -noanalysis -readOnly and pinned EXE SHA256. Output COMPLETE.
Private native-camera-re/cinema-owner-fields-20261103.txt and
cinema-owner-callers-20261103.txt remain outside Git.
New call graph: Wait callers477FE0 are A4E9B0 (returns decoded texture resources
C8/CC/D0) and9F3B30 (volume/pause/update). Therefore a Wait caller whitelist alone
would still classify texture users. FC7D00 constructors AB3E60/B391C0 identified;
do not equate RegionMgr loading-video+2E8 with all fullscreen/boot playback.
No native patch, observer hook or classification change deployed.

## Lighting investigation

Prior HUD task captured shadow-light draw VS68EEE79C/PSC4D1FB10, but old captures
lack effective PS banks and fail hand-upload validation. Do not relabel valid.
Existing cache disassembly inspected via D3DDisassemble: eight inverse-projection
shaders and six ViewExtents shaders found across archives. E51C4E92/E345B150 and
DA655BE2/45110BD0/E91460F1/092A3FF4 consume ViewExtents.z/w for depth operations;
their names alone do NOT justify inserting head inverse matrices. Most do not
use ViewExtents.xy for a reconstruction ray. Keep old failed named-matrix and
auxiliary-scene-suppression approaches rejected.

## Next steps / test gate

User confirms the lighting issue also occurred before the resolution trial.
Do not attribute it to3200x3264, AF16 or the latest NIS experiment. This is a
historical occurrence report, not a new physical test on the restored payload.
User will launch and signal readiness at a visible affected surface; wait for
that signal before requesting any capture. No current game process at last check.
No new physical result, no image-fix candidate and no FPS improvement claimed.
If reproduced, use existing matched one-frame capture to obtain effective PS
banks and matching shader/depth/light inputs at two head poses. Current recorder
supports lighting_ps_constants; no need to reimplement it or request old save.
First verify identities and game/readiness before requesting capture; don't
launch or inject input without authorization. Never use capture FPS as baseline.
For cinema continue native full-screen presentation ownership, not arbitrary
resolution threshold, timeout, or absence of world camera. For next runtime
candidate base rendering code on accepted81bddf2b; retain exact rollback above.

## Authorized matched captures, 2026-11-03

User signaled readiness twice. Actual process43596, DLL E56BF027 verified.
One request per pose: serial201/frame8420 and202/frame16299. Both complete with
original Present hr0, both eyes, matching camera-frame, stage shaders and effective
lighting banks (14 and10 rows). Preserved outside Git/Temp under
C:/Users/Trilex/AppData/Local/Darkness2VR/captures/lighting-20261103/
Darkness2VR-matched-43596-201-8420 and Darkness2VR-matched-43596-202-16299.
All receipt files independently verified after copying. Runtime log also saved.
Diagnostic INI backed up for each request and restored byte-exact to AC8290E4;
MatchedFrameCapture0. No DLL, gameplay INI or render behavior changed.
User told recording finished and free to move/play; no further automatic shots.

Initial validator rejected image/metadata ordering, despite runtime completion.
Source audit proves StartStereoTransfer calls PrepareStereoEyeConstants BEFORE
CaptureBackbufferToShared; the former logs `blit` during CPU CB preparation,
not GPU consumption. Corrected validator independently bounds input and parameter
records between source and final. All prior Present/identity/hash/draw/image
checks retained; no captured bytes edited. Added CPU-before-input acceptance
and input/parameters-after-final rejection tests. Suite34 PASS; both actual
captures now pass full file/metadata validation, projection ownership still false.
Comment clarifies recorder semantics; no binary rebuilt/deployed for this fix.

Added read-only audit_lighting_constants.py. Exact captured C4D1FB10 SHA and
native selected-camera projection plus proven native0.95 conversion establish
expected current correction. Effective c27 matrices match at all10+6 consuming
shadow draws, max absolute error1.11e-6/1.52e-6. Therefore a missing inverse-head
constant upload is not supported for THESE draws. This is delivery proof, not
correct reconstruction/depth sampling or lighting acceptance. Do not remove the
0.95 bias: AD4021..AD4067 intentionally scales projection[14], already pinned in
native_projection_upload.h and native installer. The observed near-depth
difference is expected native behavior, not newly discovered patch evidence.

Both pictures show villa floor medallion; second native camera position differs
by about1.91 game units and native heading also changes. Thus these are valid
individual frames but not a stationary-body pixel A/B. Physical eye displacement
also present. Do not claim a pure-yaw comparison or derive visual drift magnitude
from unmatched pixels. Next offline work: use consuming shader instructions,
WVP/STP/PS banks and depth encoding to test reconstruction consistency; changing
coordinates remains unjustified until actual input/producer semantics match.

Follow-up offline: tools/test_matched_capture.py PASS32. Private cached shader
D82658F7 disassembly identifies OmniShadowMap/OmniShadowLookup, LightParams c1,
DarkeningFactor c2; receives light-space vector through v0, not an inverse-matrix
PS. The existing recorder explicitly includes its effective PS bank even without
InverseProjection/ViewExtents metadata. Its vertex producer must be matched at
the affected draw before changing coordinates. No shader patch applied.
