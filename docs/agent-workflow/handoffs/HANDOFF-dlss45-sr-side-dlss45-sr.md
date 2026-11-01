# Handoff: DLSS 4.5 Super Resolution / DLAA & FSR 3.1.5/4.x Integration for Darkness2VR

**Date**: 2026-11-01
**Agent**: SIDE (Antigravity)
**Branch**: `side/dlss45-sr`
**Worktree**: `C:\Users\Trilex\Darkness2VR\.side-dlss45-sr`
**Baseline**: `88ad6b65` (`codex/gaze-nis-main-integration`)

## Goal
Prepare the complete, verified offline implementation of **DLSS 4.5 Super Resolution / DLAA** (`nvngx_dlss.dll v310.9.1.0`, Presets `K`, `L`, `M`, `J`, `E`, `F`, `PerMode=16`) and **AMD FidelityFX Upscaler (`FSR 3.1.5 / 4.x`)** for `SubmitMode=4` dual-eye stereo in `Darkness2VR`, including:
- 64-bit out-of-process D3D12 NGX/FidelityFX helper (`d2vr_dlss_host64.exe`)
- 32-bit D3D11 client (`src/dlss/dlss_client.h/.cpp`) with NT-shared textures & fences and non-blocking async startup
- D3D11 GPU guide shaders (`src/dlss/dlss_gpu.h/.cpp`) for reversed `R32_FLOAT` depth (`1/(1+z)`), `R16G16_FLOAT` motion vectors, `R8_UNORM` current-color bias mask, and 8x8 tile + 3x3 patch block-matching `objmotion` compute pass
- Proxy integration (`src/dlss_eye.inl`, `src/nis_eye.inl`, `src/d3d9.cpp`, `build.ps1`, `Darkness2VR.ini`) with seamless NIS warmup fallback and optional post-DLSS `FoveatedSharpen` (`NisMode=2`)
- Standalone x86↔x64 host verification and RTX 5070 GPU benchmark suite (`tools/dlss-host-test.ps1`, `tools/dlss-host-tests.cpp`)

## Status
**Ready for Review / MAIN Deployment** (30/30 standalone GPU/IPC checks passed on local RTX 5070; `d3d9.dll` compiled cleanly in both standard and `-DiagnosticCapture` flavors; Steam folder untouched by SIDE).

## Context Loaded
- [x] `docs/agent-workflow/BASELINE.md`
- [x] `docs/agent-workflow/FAILED-APPROACHES.md` (checked)
- [x] `docs/agent-workflow/handoffs/PLAN-dlss45-sr-integration-main.md`
- [x] `docs/agent-workflow/handoffs/PLAN-dlss5-feeder-integration-main.md`

## Changes
- **Files added**:
  - `src/dlss/dlss_ipc.h` — Packed x86/x64 wire contract (`kMagic = 0x52563244u` `'D2VR'`, `kVersion = 3`, pipe `\\.\pipe\d2vr-dlss.%lu`, `Hello=28B`, `HelloAck=348B`, `Build=96B`, `BuildAck=140B`, `Frame=52B`, `FrameAck=24B`)
  - `src/dlss/dlss_client.h` & `src/dlss/dlss_client.cpp` — 32-bit D3D11 client (`d2vr::dlss::Client`) with NT-handle shared textures (`Color`, `Output`, `Depth`, `Motion`, `Bias`), shared `ID3D11Fence`s (`in`, `out`), `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, and bounded pipe wait before queuing `ctx4->Wait(fenceOut)`
  - `src/dlss/dlss_gpu.h` & `src/dlss/dlss_gpu.cpp` — D3D11 `GuideGpu` shaders (`vsmain`, `psmain`, `ps_mask`, `ps_audit`, `ps_flow`, `cs_objpre`, `cs_objsearch`, `ps_objfix`)
  - `src/dlss/dlss_jitter.h` — Halton (2,3) projection jitter math and verified sign constants (`kReportX = -1`, `kReportY = -1`)
  - `src/dlss_eye.inl` — Per-eye stereo integration for `SubmitMode=4` with background worker thread initialization, NIS warmup upscaling (`zero dropped frames`), and `R8G8B8A8_UNORM` -> `B8G8R8A8_UNORM` output conversion for OpenXR swapchains and `NisEyeProcess`
  - `src/tools/dlss_host/dlss_host.cpp` & `src/tools/dlss_host/NOTICE.md` — 64-bit D3D12 NGX / FidelityFX helper (`d2vr_dlss_host64.exe`)
  - `tools/fetch-ngx.ps1`, `tools/fetch-ffx.ps1`, `tools/build-dlss-host.ps1`, `tools/dlss-host-test.ps1`, `tools/dlss-host-tests.cpp` — SDK fetchers, x64 host builder, and 30-check x86↔x64 GPU test & benchmark runner
  - `docs/agent-workflow/handoffs/PLAN-dlss45-sr-integration-main.md` & `docs/agent-workflow/handoffs/PLAN-dlss5-feeder-integration-main.md`
- **Files modified**:
  - `src/d3d9.cpp` — Wired `dlss_eye.inl` into `LoadConfig()`, `EnsureSwapchains()`, `SubmitXR()`, `Hook_Reset()`, and `Hook_ResetEx()`
  - `src/nis_eye.inl` — Added `forceScaleToSwapchain` parameter to `NisEyeProcess` so NIS upscales smaller eye caches while DLSS warms up asynchronously
  - `build.ps1` — Added `src\dlss\dlss_client.cpp` and `src\dlss\dlss_gpu.cpp` to `cl.exe` build, plus automatic deployment of `build\dlss_host\*` into `<GameDir>\d2vr_dlss\` when `-Install` is run by `MAIN`
  - `Darkness2VR.ini` — Documented `[Render]` keys (`DlssMode=0`, `DlssPreset=16`, `DlssBackend=0`, `DlssObjectMotion=1`, `DlssBiasMask=0`)
  - `.gitignore` — Excluded `/build/dlss_host/`, `/third_party/ngx/`, and `/third_party/ffx/` so third-party redistributable binaries are never committed

## Evidence

### 1. Standalone x86↔x64 Host Test Suite (`30 passed, 0 failed`)
Executed `powershell -ExecutionPolicy Bypass -File tools/dlss-host-test.ps1` on `NVIDIA GeForce RTX 5070` (driver `32.0.15.9571`, `nvngx_dlss.dll v310.9.1.0`, AMD FidelityFX SDK `2.3.0` / `FSR 3.1.5`):
- `client.start` (`featureOk=1`, two independent NGX handles created on matching LUID)
- Two-eye temporal isolation (`eye 1` constant green `G=204.0` unaffected by `eye 0` horizontal edge sweep)
- Motion vector sign (`prev - cur` in UV space: `mv+` = `0.95` vs `mv-` = `25.00`)
- Projection jitter sign (`kReportX = -1`, `kReportY = -1`, err `2.06 < 12.0`)
- Hard-cut `reset=true` flushes temporal history in 1 frame (`G=0.00`)
- `GuideGpu` reversed depth (`far=0.048 < near=0.333`), rotation flow, hard-cut reset, bias mask (`disagree=255 > agree=0`), and `objmotion` compute block matching (`vx = +6.00 px`, `mad = 1.99`, `overridden = 171`)
- Super Resolution (`512x512 -> 768x768` Quality 1.5x, `512x512 -> 666x666` UltraQuality 1.3x)
- All DLSS 4.5 presets verified (`10=J`, `11=K`, `12=L [DLSS 4.5]`, `13=M [DLSS 4.5]`, `16=PerMode`)
- Helper crash / kill fail-fast recovery (`TerminateProcess` -> `evaluate` returns `false` in **1 ms** with no GPU fence hang)
- AMD FidelityFX `BackendFsr` (`FSR 3.1.5`, two-eye isolation `G=203.9`, SR `512->768`, kill fail-fast `1 ms`)

### 2. Measured RTX 5070 Per-Eye GPU Timings at Quest 3 OpenXR Resolution (`2676x2728` output)
Measured via `build\dlss_host\dlss-host-tests.exe --cost` (60 stereo frames after warmup, `gpuMs` via D3D12 timestamp queries per eye; multiply by 2 for full stereo pair):

| Preset / Backend | Performance (`1338x1364 -> 2676x2728`) | Balanced (`1574x1604 -> 2676x2728`) | Quality (`1784x1818 -> 2676x2728`) | DLAA / Native (`2676x2728 -> 2676x2728`) |
| :--- | :---: | :---: | :---: | :---: |
| **Preset K (`11`, Transformer)** | **1.87 ms / eye** (3.74 ms pair) | **1.92 ms / eye** (3.84 ms pair) | **1.89 ms / eye** (3.78 ms pair) | **1.95 ms / eye** (3.90 ms pair) |
| **Preset 16 (`PerMode`: K / M / L)** | **2.50 ms / eye** (Preset M) | **1.94 ms / eye** (Preset K) | **2.01 ms / eye** (Preset K) | **2.24 ms / eye** (Preset K) |
| **Preset M (`13`, DLSS 4.5 Dynamic)** | **2.54 ms / eye** (5.08 ms pair) | **3.18 ms / eye** (6.36 ms pair) | **3.78 ms / eye** (7.56 ms pair) | **7.29 ms / eye** (14.58 ms pair) |
| **Preset L (`12`, DLSS 4.5 UltraPerf)** | **3.10 ms / eye** (6.20 ms pair) | **3.99 ms / eye** (7.98 ms pair) | **4.68 ms / eye** (9.36 ms pair) | **9.18 ms / eye** (18.36 ms pair) |
| **Preset F (`6`, Fast CNN)** | **0.79 ms / eye** (1.58 ms pair) | **0.78 ms / eye** (1.56 ms pair) | **0.80 ms / eye** (1.60 ms pair) | **0.87 ms / eye** (1.74 ms pair) |
| **AMD FSR 3.1.5 (`DlssBackend=1`)** | **0.80 ms / eye** (1.60 ms pair) | **0.86 ms / eye** (1.72 ms pair) | **0.98 ms / eye** (1.96 ms pair) | **1.32 ms / eye** (2.64 ms pair) |

- **Guide + Object Motion + Bias Mask D3D11 overhead at `2676x2728`**:
  - `guide` (`run`): **0.07 ms / eye**
  - `guide + mask`: **0.19 ms / eye**
  - `guide + objmotion`: **0.46 ms / eye**

### 3. Proxy Build & Gate Verification
- Standard build (`build.ps1`): `0 warnings, 0 errors`
- Protected-native build (`build.ps1 -DiagnosticCapture`): `0 warnings, 0 errors`, verified by `tools/assert-candidate-profile.ps1` (`Flavor: protected-native`, `SHA256: 138AAA969F6F1D9052BEF9E457A7613AB121DE4DCDACAE69DF913430FFB1EE07`)

## Next Steps for MAIN
1. **Merge or checkout `side/dlss45-sr`** (or cherry-pick the commit from `side/dlss45-sr`).
2. **Build the 64-bit helper (if building in the main worktree)**:
   - Note: `C:\Users\Trilex\Darkness2VR\.side-dlss45-sr\build\dlss_host\` already has the built `d2vr_dlss_host64.exe`, `nvngx_dlss.dll` (`v310.9.1.0`), `amd_fidelityfx_loader_dx12.dll`, and `amd_fidelityfx_upscaler_dx12.dll`.
   - Or run `powershell -ExecutionPolicy Bypass -File .\tools\build-dlss-host.ps1` in the main worktree.
3. **Install to the Steam game folder (`MAIN ONLY`)**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build.ps1 -DiagnosticCapture -Install
   ```
   This automatically deploys `d3d9.dll` to the game root and copies `d2vr_dlss_host64.exe` + `nvngx_dlss.dll` + FSR DLLs into `<GameDir>\d2vr_dlss\`.
4. **Recommended INI profiles for in-headset A/B testing (`Darkness2VR.ini`)**:
   - **Profile A — DLSS 4.5 Quality + Preset K + Foveated NIS Sharpen (Recommended for 90 Hz / 120 Hz)**:
     ```ini
     [Render]
     SubmitMode=4
     DlssMode=2          ; Quality (1.50x upscale to OpenXR swapchain)
     DlssPreset=11       ; Preset K (Transformer, ~1.89 ms/eye on RTX 5070)
     DlssBackend=0       ; NVIDIA NGX DLSS
     DlssObjectMotion=1  ; 8x8 block-matching motion vectors
     DlssBiasMask=0
     NisMode=2           ; Optional post-DLSS FoveatedSharpen (set 0 for pure DLSS)
     ```
   - **Profile B — DLSS 4.5 Performance + Preset M (2nd-Gen DLSS 4.5 Dynamic Transformer)**:
     ```ini
     [Render]
     SubmitMode=4
     DlssMode=4          ; Performance (2.00x upscale)
     DlssPreset=16       ; PerMode (automatically selects DLSS 4.5 Preset M for Performance, ~2.50 ms/eye)
     DlssBackend=0
     DlssObjectMotion=1
     ```
   - **Profile C — DLAA Native + Preset K (Maximum anti-aliasing at full resolution)**:
     ```ini
     [Render]
     SubmitMode=4
     DlssMode=1          ; DLAA (1.00x native resolution)
     DlssPreset=11       ; Preset K (~1.95 ms/eye on RTX 5070)
     DlssBackend=0
     DlssObjectMotion=1
     ```

## Blockers
None.
