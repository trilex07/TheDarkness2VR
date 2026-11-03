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

User was asked whether issue persists on restored accepted payload; pending.
No new physical result, no image-fix candidate and no FPS improvement claimed.
If reproduced, use existing matched one-frame capture to obtain effective PS
banks and matching shader/depth/light inputs at two head poses. Current recorder
supports lighting_ps_constants; no need to reimplement it or request old save.
First verify identities and game/readiness before requesting capture; don't
launch or inject input without authorization. Never use capture FPS as baseline.
For cinema continue native full-screen presentation ownership, not arbitrary
resolution threshold, timeout, or absence of world camera. For next runtime
candidate base rendering code on accepted81bddf2b; retain exact rollback above.
