# Baseline

**Accepted baseline** - update only when change is tested and accepted by MAIN.  
**Do NOT assume Git `main` is the deployed baseline.**

## Accepted Steam Binary
- **Installed DLL (Steam)**: `C:\Program Files (x86)\Steam\steamapps\common\Darkness II\d3d9.dll`
- **Installed DLL SHA256**: 416F710996324C7572863F122E1A1F842A3DFEBE5D3316D055A5A857AA5F8F14
- **Installed INI SHA256**: CFBBE0A850866E369535B79BF1F4D3A730B006278D39CA6050181E8DBCB36822
- **Deployed DLL source commit**: unknown (exact build provenance not available)
- **Last known good backup**: Check `d3d9.dll.off-*` in game dir (e.g. probe/lightfix/historyfix entries)
- **Deployment**: MAIN only via `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install` (creates timestamped backups)

## Verified Source & Integration
- **Verified source commit (for accepted build)**: unknown (documentation commit 1051c5c6 is not proven to be the deployed binary source)
- **Integration branch (accepted)**: origin/main (1051c5c6). **Not guaranteed to match deployed DLL** - verify hashes.
- **Repo DLL SHA256**: 9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C
- **Repo INI SHA256**: 76EF2369F50ACC7E94ECF7503A0C8BF4199221CDE49FAAB7F1D4652DB2C0E8C5
- **Current workflow branch**: agent/workflow-optimization
- **Workflow base**: Workflow-only changes; no unrelated runtime history pulled

## Environment
- **Game**: The Darkness II (Steam AppID 67370), x86 D3D9Ex, Evolution Engine
- **Branch (current)**: agent/workflow-optimization
- **Config (repo)**: SubmitMode=4, StereoMode=1, HeadGeometry=1, NativeTrackedHands=0
- **Logs**: %LOCALAPPDATA%\Darkness2VR\d3d9.log
- **Build**: `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install` (MAIN only)

## Working Features (from latest accepted evidence)
- [ ] D3D9Ex proxy, vtable hooks, OpenXR session/swapchains *(unverified)*
- [ ] SubmitMode 2/3 cinema modes *(unverified)*
- [ ] VR gamepad emulation *(unverified)*
- [ ] SubmitMode 4 same-frame stereo (record+replay) *(unverified)*
- [ ] Hotkeys: F6 StereoSep, F11 StereoConv, F10 cycle SubmitMode *(unverified)*
- [ ] Native aim/weapon hooks (RC6/RC7 family) *(experimental/unverified - may be quarantined)*

**Mark features as verified only after MAIN in-game testing with evidence.**

## Known Issues
- True stereoscopic depth/6DoF not fully working in all cases
- FOV override not active (RE incomplete) - missing top/bottom coverage
- AFR rejected due to temporal mismatch
- Bone matrix corruption if scanning beyond c0..c3
- Some native hand patches quarantined/rejected (e.g. 66B3CA)
- Lighting/shadow parity issues reported
- Performance varies by scene

## Experimental/Unverified Branches
Many `codex/*`, `feature/*`, `research/*` branches exist - treat as experimental unless merged and accepted by MAIN with evidence.

## Update Instructions
When accepting a new baseline (MAIN only):
1. Verify installed DLL hash in Steam game dir
2. Record deployed DLL source commit if exact build provenance available
3. Record integration branch
4. Snapshot relevant INI settings/hashes
5. Document tested features with evidence (logs/screens)
6. Commit baseline update with evidence
7. Never auto-assume Git `main` matches deployed binary
8. Do not claim documentation commits as verified binary source
