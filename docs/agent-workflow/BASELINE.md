# Baseline

**Accepted baseline** - update only when change is tested and accepted.

## Current
- **Commit**: cb6638fc (Match OpenXR FOV to rendered symmetric eye projections)
- **Branch**: main (current working tree has modifications)
- **DLL (repo root)**: d3d9.dll
- **DLL SHA256**: 9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C
- **Config**: Darkness2VR.ini (SubmitMode=4, StereoMode=1, HeadGeometry=1, NativeTrackedHands=0)
- **Game**: The Darkness II (Steam AppID 67370)
- **Focus**: SubmitMode=4 same-frame stereo (record+replay), FOV/culling
- **Logs**: %LOCALAPPDATA%\Darkness2VR\d3d9.log
- **Build**: `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install`

## Working Features (from handoffs)
- D3D9Ex proxy, vtable hooks, OpenXR session/swapchains
- SubmitMode 2/3 cinema modes work
- VR gamepad emulation works
- SubmitMode 4 same-frame stereo implemented (record+replay)
- Hotkeys: F6 StereoSep, F11 StereoConv, F10 cycle SubmitMode

## Known Issues
- True stereoscopic depth/6DoF not fully working in some cases
- FOV override not active (RE incomplete) - missing top/bottom coverage
- AFR rejected due to temporal mismatch
- Bone matrix corruption if scanning beyond c0..c3

## Modified Files (working tree)
- docs/research/startup-crash.md
- tools/frame-boundary-simulator/frame_boundary_suite.exe
- tools/prepare_vr_candidate.ps1

## Update Instructions
When accepting a new baseline:
1. Record commit hash, branch
2. Record DLL hash (repo and/or installed)
3. Snapshot relevant INI settings
4. Document tested features and known issues
5. Commit baseline update
