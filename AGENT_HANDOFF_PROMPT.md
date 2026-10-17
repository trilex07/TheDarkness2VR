# Darkness2VR: Technical Handoff & Agent Engineering Prompt

## 1. Mission Objective & Status Summary
- **Game**: *The Darkness II* (Steam AppID: `67370`, Steam path: `C:\Program Files (x86)\Steam\steamapps\common\Darkness II`)
- **Engine**: Digital Extremes Evolution Engine (x86 32-bit, DirectX 9Ex)
- **Target VR System**: OpenXR runtime (SteamVR / PSVR2 / Quest via Link)
- **Project Location**: `c:\Users\Trilex\Darkness2VR` (Git branch `main`, remote `origin/main` -> `trilex07/TheDarkness2VR`)
- **Current State**:
  - The OpenXR bridge, frame capture pipeline, cinema screen modes (world-anchored & head-locked), and virtual gamepad emulation (OpenXR actions -> XInput) work reliably without crashing.
  - However, **in-game stereoscopic 3D depth and 6DoF camera tracking are NOT yet working correctly**. Alternate Frame Rendering (AFR) via shader constant manipulation either causes rendering corruption or fails to provide acceptable, fused 3D stereoscopic depth.

---

## 2. Codebase & Development Environment
- **Proxy DLL**: `d3d9.dll` compiled via MSVC `cl.exe` (x86 32-bit `/O2 /std:c++17`).
- **Build Script**: `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install`
  - Compiles `src\d3d9.cpp` and `src\d3d9.def`.
  - Links against `openxr_loader.lib`, `dxgi.lib`, `d3d11.lib`, `user32.lib`, `shell32.lib`, `xinput.lib`.
  - Backs up old DLL to `d3d9.dll.off-<timestamp>` and installs new `d3d9.dll` + `openxr_loader.dll` directly to the Steam game folder.
- **Config File**: `Darkness2VR.ini` (in game root and repo root).
- **Runtime Log**: `%LOCALAPPDATA%\Darkness2VR\d3d9.log`.

---

## 3. What Works (Verified Stable)
1. **D3D9Ex Proxy & Device Hooking**:
   - `Direct3DCreate9` / `Direct3DCreate9Ex` exported via `src\d3d9.def`.
   - VTable hooks on `IDirect3DDevice9Ex`:
     - `Present@17`, `Reset@16`, `BeginScene@41`, `EndScene@42`, `SetVertexShaderConstantF@94`, `PresentEx@121`, `ResetEx@132`.
   - Non-intrusive frame interception at `PresentEx@121`.
2. **OpenXR Session & Presentation (`SubmitXR`)**:
   - Initializes OpenXR session with D3D11 graphics binding.
   - Dual stereo swapchains created with format `DXGI_FORMAT_B8G8R8A8_UNORM` (or SRGB).
   - Frame capture: `dev->GetRenderTargetData(back, g_sysmem)` -> CPU map / copy to `g_upload` dynamic D3D11 texture -> write to OpenXR swapchain.
3. **Cinema Modes (Modes 2 & 3)**:
   - `SubmitMode 2` (`QUAD_WORLD_ANCHOR`): World-anchored cinema quad using level yaw/position latching. Works stably for menus and 2D cutscenes.
   - `SubmitMode 3` (`QUAD_HEAD_LOCKED`): Head-locked HUD/cinema quad.
4. **VR Gamepad Emulation**:
   - Hooks `xinput1_3.dll` / `xinput1_4.dll` (`XInputGetState`).
   - Polls OpenXR thumbsticks, triggers, and buttons, injecting them into the game as physical gamepad state.

---

## 4. What Was Attempted & Why It Failed (Empirical Evidence)

### A. Bone Matrix Corruption (The "Scrambled Meshes" Bug)
- **What was done**: An early implementation scanned vertex shader constant registers up to register 64 or 256, looking for 4x4 matrices that met perspective camera invariants (orthonormal columns).
- **Observed failure**: Player hands, weapons, demon tentacles, and enemy bodies were violently spiked, distorted, and flickering on alternating frames.
- **Root cause verified in logs**:
  The Evolution Engine skeletal mesh palette is uploaded starting at register `c45` or `c52` (`Vector4fCount == 168` or `75`). Skeletal bone matrices are orthonormal rotation/translation matrices ($|\text{col}| \approx 1.0$), so the naive matrix scanner falsely matched 13+ bone matrices per frame (`c52, c64, c68, c76, c88, c100, c112, c124...`) and translated them as if they were camera matrices.
- **Current Mitigation**: Hook is locked strictly to `StartRegister == 0` (`c0..c3`). This stopped the mesh distortion, but did not solve true stereo 3D.

### B. Matrix Modification via `SetVertexShaderConstantF` (`c0..c3`)
- **What was done**: In `Hook_SetVertexShaderConstantF`, when `StartRegister == 0` and column vectors match the perspective projection ($P_x \approx 2.537, P_y \approx 4.511$, aspect $\approx 1.778$, Col 3 forward $\approx 1.0$), Row 3 was modified to displace the camera:
  $$\text{Row}_3 \leftarrow \text{Row}_3 - (o_x \text{Row}_0 + o_y \text{Row}_1 + o_z \text{Row}_2)$$
  where $\vec{o} = \pm 0.035\text{m} \cdot \vec{\text{Right}}$.
- **Observed failure**:
  1. The user reported that either both eyes saw the same flat image, or there was no stereoscopic depth and severe visual desynchronization / retinal rivalry.
  2. Deferred rendering artifacts: Cel-shading outlines, screen-space ambient occlusion, deferred light volumes, and decals were misaligned or rendered flat.
- **Root cause**:
  1. **Evolution Engine is a Deferred Renderer**: It renders geometry to multiple render targets (G-Buffers). Screen-space lighting, shadows, and cel-shading outlines reconstruct world/view coordinates from the G-Buffer depth buffer using inverted camera matrices or constants passed in subsequent passes. Modifying only the vertex shader `c0..c3` during geometry pass causes G-Buffers and deferred lighting/outlines to decouple.
  2. **Multi-Constant Uploads**: At `c0`, `Vector4fCount` is often 12 or 17. The engine passes ViewProj, View, and Camera World Position (`vEyePos`). Modifying only `c0..c3` leaves lighting shaders with the old camera origin.
  3. **AFR Temporal Disparity**: Alternate Frame Rendering on a 60Hz/90Hz game means eye $L$ is rendered at time $t$ and eye $R$ is rendered at time $t + 16\text{ms}$. When the player turns their head or objects move, the brain cannot fuse the stereoscopic image due to temporal parallax and retinal rivalry.

---

## 5. Reverse-Engineering Discoveries (Evolution Engine)

```text
Game executable: DarknessII.exe (32-bit x86, Steam AppID 67370)
D3D Interface: Direct3DCreate9Ex -> IDirect3DDevice9Ex
Rendering Passes: 40 to 160 SetVertexShaderConstantF calls per frame

Camera Matrix Invariants at c0..c3:
  Row 0: [ M00  M01  M02  M03 ]  where (M00, M10, M20) = Right * Px   (Px ~ 2.537)
  Row 1: [ M10  M11  M12  M13 ]  where (M01, M11, M21) = Up * Py      (Py ~ 4.511)
  Row 2: [ M20  M21  M22  M23 ]  where (M02, M12, M22) = Depth scale
  Row 3: [ M30  M31  M32  M33 ]  where (M03, M13, M23) = Forward      (len ~ 1.000)

Aspect Ratio: Py / Px = 4.511 / 2.537 = 1.7778 (Exact 16:9)
Coordinate Units: Engine world units (Row 3 values range from tens to hundreds in game levels).
```

---

## 6. Recommended Next Engineering Directions (Ranked)

### Approach 1 (Highest Quality): Native Engine Camera Memory Hook
Instead of intercepting D3D9 vertex shader constants, locate and hook the engine's native camera function:
1. Attach x32dbg or Cheat Engine to `DarknessII.exe`.
2. Search for the camera position and orientation (floats corresponding to camera $X, Y, Z$, Yaw, Pitch, Roll).
3. Set a hardware breakpoint on write to find the native camera update function (e.g. `Camera::Update`, `PlayerView::UpdateMatrix`).
4. **Hook this function in `Darkness2VR`**:
   - Write VR headset orientation (Pitch, Yaw, Roll) and position ($X, Y, Z$) directly into the camera structure.
   - For stereo: Either duplicate the camera render pass (Dual Pass) or let the engine natively compute all G-Buffers, lighting, outlines, and decals from the true 6DoF camera.

### Approach 2 (Fastest Working 3D): DIBR Depth Buffer Synthesis (ReShade / SuperDepth3D Style)
In deferred renderers, Depth-Image-Based Rendering (DIBR) provides 100% synchronized, artifact-free stereo:
1. In `Hook_PresentEx`, intercept the game's resolved Depth-Stencil surface or G-Buffer depth texture (Evolution Engine writes depth to a high-precision target or D3D9 depth buffer).
2. Create an offscreen D3D11 pixel/compute shader.
3. Reproject the single rendered frame into Left and Right eye views simultaneously using the depth map and disparity formula:
   $$\Delta x = \frac{\text{IPD} \cdot (\text{depth} - \text{convergence})}{2 \cdot \text{depth}}$$
4. **Why this works**:
   - Zero temporal eye mismatch (both eyes are generated from the exact same frame instant).
   - Zero decal or cel-shading corruption.
   - 100% stable framerate and instant stereoscopic 3D depth perception.

### Approach 3 (Classic D3D9 VR Modding): Geo-11 / 3D Vision Bytecode Patching
1. Hook `IDirect3DDevice9::CreateVertexShader`.
2. Disassemble incoming vertex shader bytecode.
3. Identify the position output instruction (e.g. `dp4 oPos.x, v0, c0` or `m4x4 oPos, v0, c0`).
4. Inject stereoscopic separation instruction before clipping:
   $$oPos.x \leftarrow oPos.x \pm (\text{Separation} \cdot (oPos.w - \text{Convergence}))$$
   This is the proven method used by Geo-11 / HelixMod for hundreds of DirectX 9 titles.

---

## 7. Immediate Instructions for the Next Agent
1. Inspect `src/d3d9.cpp` and `Darkness2VR.ini` in `c:\Users\Trilex\Darkness2VR`.
2. Check `%LOCALAPPDATA%\Darkness2VR\d3d9.log` to see runtime output and matrix dumps.
3. Reference `c:\Users\Trilex\vr-mods\SingularityVR-production` for proven UE3/D3D9 OpenXR mod implementations.
4. Pick **Approach 1** (Engine Camera Hook) or **Approach 2** (DIBR Depth Shader) to achieve true, comfortable stereoscopic 3D without AFR retinal rivalry.
