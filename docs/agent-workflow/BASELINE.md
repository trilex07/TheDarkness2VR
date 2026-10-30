# Baseline

**Accepted baseline** - update only when change is tested and accepted by MAIN.

## Current (Verified)
- **Repo HEAD (workflow branch)**: 3275d1de (agent/workflow-optimization)
- **Latest MAIN commit**: 1051c5c6 (Add comprehensive AGENT_HANDOFF_PROMPT.md...)
- **Installed DLL (Steam)**: `C:\Program Files (x86)\Steam\steamapps\common\Darkness II\d3d9.dll`
- **Installed DLL SHA256**: 416F710996324C7572863F122E1A1F842A3DFEBE5D3316D055A5A857AA5F8F14
- **Repo DLL SHA256**: 9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C
- **Repo INI SHA256**: 76EF2369F50ACC7E94ECF7503A0C8BF4199221CDE49FAAB7F1D4652DB2C0E8C5
- **Installed INI SHA256**: CFBBE0A850866E369535B79BF1F4D3A730B006278D39CA6050181E8DBCB36822
- **Branch (current)**: agent/workflow-optimization
- **Game**: The Darkness II (Steam AppID 67370), x86 D3D9Ex, Evolution Engine
- **Config (repo)**: SubmitMode=4, StereoMode=1, HeadGeometry=1, NativeTrackedHands=0
- **Logs**: %LOCALAPPDATA%\Darkness2VR\d3d9.log
- **Build**: `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install` (creates `d3d9.dll.off-<timestamp>` backups)

## Working Features (verified from handoffs/logs)
- D3D9Ex proxy, vtable hooks, OpenXR session/swapchains
- SubmitMode 2/3 cinema modes work
- VR gamepad emulation works
- SubmitMode 4 same-frame stereo (record+replay) implemented
- Hotkeys: F6 StereoSep, F11 StereoConv, F10 cycle SubmitMode
- Native aim/weapon hooks (RC6/RC7 family) in some states

## Known Issues
- True stereoscopic depth/6DoF not fully working in all cases
- FOV override not active (RE incomplete) - missing top/bottom coverage
- AFR rejected due to temporal mismatch
- Bone matrix corruption if scanning beyond c0..c3
- Some native hand patches are quarantined/rejected

## Recent Branch Activity (relevant)
- Many `codex/*` branches with native/render optimizations (see git branch -a)
- `feature/*` branches for render pipeline work
- `research/*` branches for engine analysis

## Update Instructions
When accepting a new baseline (MAIN only):
1. Verify installed DLL hash in Steam game dir
2. Record repo DLL/hash, INI/hash
3. Snapshot relevant INI settings
4. Document tested features and known issues
5. Commit baseline update with evidence
6. Only MAIN performs deployment/testing
