# Handoff: World objective markers and right upper face-button hold

**Date**: 2026-11-02
**Task ID**: world-objective-marker-20261102
**Role**: MAIN
**Branch**: codex/world-objective-marker-20261102
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: installed source f5bdb1e8, receipt cinematic-theater-20261102T182344252,
DLL99DAF70040D3EC328D31FA3483B0C43B57DBBE7DC035EA4AAE1E50F209B295F7.
User says 'ta okej' before reporting objective markers following head. No main
merge requested for the latest source. Accepted Git main remains897c5c44.
**Integration commit**: pending

## Goal / status

OPEN: Mission destination marker should stay at its real world position rather
than follow the headset. Additional user request: hold circle/right upper VR
face button to show mission objective and markers if the hold is unused.

Button candidate READY OFFLINE: right g_actB has only native B level binding,
no existing hold. Add named ObjectiveButton action: gameplay tap -> native B
120ms pulse after release; hold1second -> stock BACK/ShowObjective120ms once,
with no B action. Menus retain immediate B. Context ownership latches per press;
focus/input loss, discontinuous polling and menu entry cancel pending hold/tap.
Settings continues to receive raw B before gameplay routing. Existing palm
ShowObjective gesture, triangle and L3+R3 minute override retained.
README documents the binding and tap-on-release behavior.

## Context loaded

Root AGENTS, bootstrap, BASELINE, WORKFLOW, relevant FAILED-APPROACHES,
HUD research and existing protected-native source. Skills: VR mod wizard,
mod reverse engineering, input systems. No SIDE delegation, game launch, input
injection or native memory modification during investigation.

## Source / binary evidence

Stock EXE SHA256D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Private extracted H_Script/D2/Menus/ObjectivePopUp.lua owns markers:
_UpdateTrackedObjectives -> gameState:GetTrackedObject(i-1) ->
avatar:GetTrackedPosition(object) -> UpdateDisplayArrow ->
movie:ProjectPosition(worldPos), then SetArrowNamePos (coordinates and distance).
Stage1280x720; script clamps arrows to edges and caches projected positions.
Shared HUD shader7070C5BC has an orthographic sprite matrix; stereo_replay.inl
only scales its X/Y and intentionally does not apply head clip or eye offset.
Do NOT apply world transforms to that entire shader: menus/subtitles share it.

ProjectPosition stringFA5618 -> binding10B1918 -> wrapperDAF8C0.
WrapperDAF986/DAF988/DAF995 dispatches movie vtable+110 with output and worldVec.
Concrete FlashInstanceImpl FDA6E8+110 ->9F5220 (ret8).
Native9F5220: movie+98 context ->99BD60 ->B2CAA0; B2CAA0 returns camera+500.
9F5349 calls5A6B30: matrix*worldVec, divide by clipW. Then maps NDC to movie
rectangle using movie+50/+54 offsets and+68/+6C size. This path is native 2D,
while scene world WVP receives headset correction later in GPU upload.
The code/report supports mismatch; precise correction timing/ownership is NOT
proved. No production marker transform or UI shader patch added yet.

GetGameState wrapperDE9D50 calls GameRules vtable+180.
Pinned1008248+180 ->43D030: smartref at rules+3D0, returns retained ref, ret8.
GetNumTrackedObjects wrapperDD3870 dispatches GameState+78;
GetTrackedObject wrapperDD3940 dispatches+7C.
GetTrackedPosition wrapperDC6AC0 ->7ECF60 ->avatar vtable+2B0 ->9A0BF0;
handles typed tracked objects specially. Do NOT substitute arbitrary entity
translation or guess the tracked-object vector from an opaque memory prefix.

## Files / validation

src/vr_input.h, src/d3d9.cpp: named objective hold and XR routing.
README.txt: user binding. tools/stereo-math-tests/cinematic_theater_test.cpp:
actual ObjectiveButton short/hold/one-shot/menu/context/input-loss fixtures PASS.
Existing cinema/expiry/chord/crop/owner suite PASS. DiagnosticCapture DLL build
PASS, SHA256DEA8D981B59C9C44745AAE4503E7D6F3A2A5F1D0314FFC4861D4ECC76201063B.

New tools/observe-objective-marker.py: read-only30s/10Hz recorder (max180s),
requires explicit PID/base and exact DLL/MAP/receiptSHA. Checks stock EXE,
installed DLL SHA, loaded PE header identity and GetGameState instruction pin.
Captures native camera+80/+500, native gameplay, current per-eye head clip,
XR state, and opaque GameState prefix/method identities. Prefix is forensic
only, no assumed tracked-object layout and no native method calls.
Recorder syntax compile PASS; live capture NOT RUN (game currently closed).

## Next gate

Install button candidate via exact previous receipt182344252 only with game
closed; verify backups/installed/protected hashes and commit receipt. This
candidate does NOT fix world marker anchoring. User must enable markers in a
playable scene; prepare recorder first, then ask30s headset rotation/lean.
Decode the exact tracked-object consumer and establish presentation ownership
before replacing/reprojecting only spatial markers. Preserve script clipping,
text distance, subtitles, normal UI, both eyes and accepted cinema/hands.
Physical button, marker location and native movie update ordering NOT RUN.

## Button candidate installed

Source b27698a2, DLL DEA8D981B59C9C44745AAE4503E7D6F3A2A5F1D0314FFC4861D4ECC76201063B.
Receipt C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T185326718.
Both targets, four protected hashes, two rollback backups verified; exact
candidate.map retained beside candidate.dll for a later read-only capture.
Game closed; no launch/input. Spatial marker correction remains OPEN.

## World marker candidate after live capture (2026-11-02)

Supersedes the earlier NOT RUN / no implementation status above. Read-only
captures head-motion.jsonl and tracked-target.jsonl are retained privately in
test-output/objective-marker-20261102. During focused samples head matrix
changed (element span 1.72836), while native camera world/VP remained constant.
Tracked GameState FD8568 methods CBA9D0/B69750 prove vector +68/+6C, stride24,
smartref +4; one target 0A80BB40 type1003348. Raw +30 is not a universal
GetTrackedPosition replacement. Native Lua typed position resolution remains.

Movie manager 10B389C -> FDA948 owns movie strong refs in +58/+5C (native
BB87C0 destructor); movie FDA6E8 has 89 executable slots, ProjectPosition at
slot68 / 9F5220, thiscall two arguments, ret8. Context FE3EE8 active +90,
smartref +34 resolves camera; B2CAA0 returns camera+500. Exact method/table
ranges are SHA256 pinned in world_marker_projection.inl.

Implementation uses existing process-lifetime InstanceTable to replace only
ProjectPosition on admitted heap movie instances, retains/releases native
strong references on the admitted window/render owner thread, preserves
original method once and original Z, all other movie methods and destructor.
Reprojects original world target with the actual same-frame headset transform
and compensates existing UI scaling. Shared UI shader is unchanged. Configuration
Render/WorldObjectiveMarkers defaults OFF; installer switch enables candidate.

Boundary: one native movie layout is shared by both eyes. This is cyclopean
world-direction correction with target depth, not per-eye marker disparity.
Native script visibility / avatar-direction edge clamp remains. Behind-camera
or invalid inputs are unchanged. Same-frame/open-frame/camera-ownership gates
are mandatory: actual Flash call ordering still needs runtime proof. Inspect
[WorldMarker] contract and corrected/refused diagnostics before claiming fix;
if all refused, determine real call ordering, never reuse stale frame matrices.

Validation: 2100 synthetic rotation/lean/depth cases against actual scene
transform PASS; actual production x86 hook ABI/native-once/null/Z and private
vtable/header/destructor preservation PASS; DiagnosticCapture full DLL build
PASS; git diff --check PASS. DLL SHA256:
B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2.
Headset / runtime correction NOT RUN. PID33840 still active at build finish.
Next: close game, install using previous receipt cinematic-theater-20261102T185326718
and -DialogueCameraComfort -WorldObjectiveMarkers; retain exact MAP alongside
DLL. Verify targets/protected/backups, then user checks target while rotating
and leaning, edge cases and unchanged menus/hands/cinema/objective hold.

## World marker candidate installed

Source d901c9c0; DLL B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2.
Receipt C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T192817000/deployment.json.
Game closed before installation; WorldObjectiveMarkers=1 and DialogueCameraComfort=1.
Verified two targets, four protected files, two rollback backups and retained exact
candidate.map. Game not launched. Runtime/headset marker correction NOT RUN.

## Marker hidden instead of reprojected (user direction)

User rejected marker projection candidate and requested hiding destination marker,
retaining objective text and purple native world guidance. Exact stock H.Script
ObjectivePopUp.lua (SHA5096a35e4809f6c255a72a301444ca79925e3176d6eab9255937663a1fd5e153)
UpdateDisplayArrow passes separate vis and arrowVis to SetArrowNamePos. Builder
replaces one comment before screen-position cache/visibility checks with:
if arrowType == ARROWTYPE_Objective then vis=false;arrowVis=false end
Padded to exact original length; serialized header/tail and every other byte
unchanged. Objective list / AddObjective, NPC/team markers and world FX untouched.
Stock compressed resource pin30d88a89d0c5bfd99a9ce2c92a66d7bfe5daa2cae2b4293eed695cd89f1d5eab.
Existing Cache writer appends one raw resource and redirects only its TOC entry;
identity/readback/unrelated TOC checks PASS. Private package:
test-output/hide-objective-marker-20261102. New deploy asset kind allows exactly
H.Script.cache/toc with backups/closed-game checks. Runtime projection disabled
through explicit installer -DisableWorldObjectiveMarkers (DLL unchanged).
Headset NOT RUN. Game process absent at preparation completion.

Installed source daa34730 with game closed. Runtime setting receipt:
C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T193328001
(DLL unchanged; WorldObjectiveMarkers=0; candidate.map retained).
Asset receipt: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/hide-objective-marker-20261102T1940/deployment.json.
Both H.Script files and runtime receipt verified after install; protected files
and two asset backups verified by installer. No game launch. Headset NOT RUN.
