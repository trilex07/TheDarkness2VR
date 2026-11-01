# LOD 2.0 and gaze NIS trial

**Date**: 2026-11-01  
**Task ID**: lod2-fovea-nis-20261101  
**Role**: MAIN  
**Branch**: codex/gaze-nis-main-integration  
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR  
**Baseline**: installed `999047FBE28943C29FE5C06B601B657B5122DC0B88915FC4AC18B65BB17C24F5`, source `7313549d`, `EngineLODScale=1.25`; see BASELINE.md.  
**Status**: In Progress

## Goal

User requests a trial of native LODScale 2.0, no NIS outside gaze, and actual 2x resolution in the gaze region.

## Evidence and scope

- The protected stock native selector applies `Graphics.LODScale` at `0x0110E074`; existing runtime trial verified the selector and scale 1.25, and user reported smoother play and correct image. This is whole-scene LOD, not gaze-selective LOD.
- Existing NIS is a postprocess on completed D3D11 eye images (`src/nis_eye.inl`). `FoveatedCopyBase=1` copies the source image and dispatches sharpening only in two gaze rectangles. Thus `FoveaPeriphery=0`, `NisSharpness=0`, `FoveaSharpness=1` gives an unsharpened periphery and a sharpened gaze region; it does not add source pixels or 2x real resolution.
- Native dynamic region rendering would require a third scene render or other upstream source path with verified camera/projection, depth, lighting, UI and same-frame compositing. Prior research (`docs/research/psvr2-eye-tracking-recon.md`) identified an inset replay as extra CPU work on an already CPU-bound frame. Do not represent NIS upscaling as real detail or enable an unverified inset.
- Extend the guarded `EngineLODScale` parser from maximum 1.5 to 2.0. Package a candidate with active game INI keys `EngineLODScale=2.0`, `FoveaPeriphery=0`, `FoveaSharpness=1.0`, `NisSharpness=0`; retain `NisMode=2` and `FoveatedCopyBase=1`.

## Gates

1. Protected-native x86 build and profile checks.
2. Full nine-target rollback, exact installed DLL/stock EXE/profile hashes, game closed.
3. User headset check of image quality, edge transitions, FPS and R2; log LOD and NIS effective state. Genuine 2x gaze-region rendering remains a separate upstream research/implementation gate.

## Installation and source-image bound

- Source commit `bc8e57cc`; protected-native x86 build/profile preflight passed. Installed candidate DLL `A174A1199450BBE1C3D7933E81611EC0FFFA8165B3AFC54C651A4DB0EFCF5DD1` at `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/lod2-gaze-nis0-20261101T120551448`. Active game INI values, stock EXE hash and all nine rollback backup hashes verified. Game not launched by agent.
- Prior runtime log showed NIS `source=2676x2728 output=2676x2728`; this proves the current sharpen path uses equal pixel counts and cannot meet a request for genuine 2x source resolution. A regional engine render remains unimplemented and requires its own evidence gate.
