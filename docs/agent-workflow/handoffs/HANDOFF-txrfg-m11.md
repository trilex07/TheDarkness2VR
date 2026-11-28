# HANDOFF: txrfg-m11

- **Branch**: `features/darkness2-txrfg-m11`
- **Role**: MAIN
- **Reference**: PreSequelVR `0f992ed` + TXRFG Win32 `5804dc0`
- **Status**: `L2_PASS` / `READY_FOR_PHYSICAL_ACCEPTANCE` (Steam install staged; deferred due to active `DarknessII.exe` PID 18528)

## Summary
- Completed TXRFG Win32 OpenXR runtime integration (`src/txfg_xr_runtime.h`, `src/txfg_runtime_state.h`, `src/txfg_d3d11_bridge.h`, `src/d3d9.cpp`) and dedicated **Optymalizacja** VR Settings tab (`src/vr_settings.h`, `src/vr_settings_config.inl`, `src/vr_settings_renderer.inl`) for SubmitMode=4 dual-eye stereo.
- **Default OFF (`Zalecany / Recommended`)**: zero extra VRAM, zero GPU dispatches, bit-exact passthrough until enabled in VR Settings -> `Optymalizacja`. Settings persist to `%LOCALAPPDATA%\Darkness2VR\txrfg-optimization.ini`, leaving `Darkness2VR.ini` and `Darkness2VR-diagnostic.ini` untouched.
- **Depth/MV investigation**: Evolution Engine D3D9Ex exposes INTZ depth only via 480x270 CPU-locked downsample (`CaptureIntzDepthToD11`, used for Method 2/F5 visualization) and no per-eye GPU depth/motion-vector buffers for SubmitMode=4; Depth/MV HSFG and M10 Neural Repair remain strictly capability-gated OFF.

## Measured Results
- **Build**: `build.ps1 -DiagnosticCapture` -> `protected-native-v1` x86 PE32 `/O2` DLL, SHA-256 `7759868D27C674D58C99283EDAD3FCE13FFC85D71C370C60C6E9C61DD67E7501` (`assert-candidate-profile.ps1` PASS).
- **TXRFG OpenXR 2x & GPU Benchmark (`tests/test_txfg_d3d11_bridge.exe`)**:
  - OpenXR 2x cadence + `XR_TIMEOUT_EXPIRED` compliance: `native=100 (45.2 fps)`, `synth=98 (44.3 fps)`, `total=89.5 fps`, `missed=2`, `sm=0x50`, `m12=0xE`.
  - Full SubmitMode=4 eye resolution (`2048x2048/eye`, incl. 4x GPU copies): `avg_gpu=0.69 ms`, `last_gpu=0.65 ms`, `avg_cpu_submit=0.67 ms`.
- **Regressions**: `test_present_overlap_contract.py` PASS (4/4), `test_present_readiness.py` PASS (8/8), `test_recorder_lock_contract.py` PASS, `test_stereo_transfer_order.py` PASS (7/7). (`test_shared_capture_color_reuse.py` had 1 pre-existing substring check failure unrelated to TXRFG.)

## Deployment & Rollback
- **Blocker**: `DarknessII.exe` (PID 18528, `D:\Darkness2VR-chapter-lab-20261128\DarknessII.exe`) was running during deployment window; Steam install left untouched (`E6B8B605...`).
- **Staged Package**: `build/txrfg-m11-20261128-191005/Install.ps1` (installs `7759868D...` with INIs untouched; `-Restore` rolls back to `E6B8B605...`).
