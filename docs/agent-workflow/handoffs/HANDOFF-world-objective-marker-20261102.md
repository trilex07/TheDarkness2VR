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
