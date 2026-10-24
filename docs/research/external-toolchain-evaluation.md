# External Reverse Engineering Toolchain Evaluation

**Darkness2VR Research Report**  
**Branch:** `research/external-toolchain`  
**Base:** `origin/research/evolution-engine-render-pipeline`  
**Author:** Tooling & Research Side Agent  

---

## 1. Executive Summary & Objective

This report evaluates existing open-source reverse engineering tools, libraries, and decompiled references for their applicability to **Darkness2VR** (Direct3D 9, Evolution Engine, 32-bit x86). It incorporates the rigorous **Matrix Adapter Accuracy Gate** to ensure diagnostic capabilities provide authoritative engineering evidence.

### Projects Evaluated

| Tool / Repository | Primary Focus | Evaluated Capability | Integration Feasibility |
|---|---|---|---|
| **phunkaeg/apitrace-mcp** | Graphics RE MCP Server | Matrix classification methodology, camera tracking, D3D call inspection | **High** (clean-room mathematical implementation in local adapter) |
| **apitrace/apitrace** | Graphics API Tracer & Replayer | 32-bit D3D9 whole-run tracing and call stream dump | **High** (external tool for MAIN baseline captures) |
| **effcol/wiz3D** | Stereoscopic 3D Driver (iZ3D continuation) | ShaderOverride loader, CRC32, ShaderHunter, Stereo Texldl scan | **Medium-High** (architectural patterns & algorithms) |
| **Puxtril/LotusLib** | Evolution Engine Cache Reader | Archive TOC structure analysis, LZF/Oodle chunk decompression | **Low** (read-only; structure compatibility verified, but unverified on real D2 archives) |
| **Puxtril/Warframe-Exporter** | Asset Exporter & ImHex Patterns | Binary format patterns (`.hexpat`) for Evolution Engine assets | **Medium** (format reference only; no repacking) |
| **FromDarkHell/TheDarknessIIDecompiled** | Decompiled Lua/Flash Scripts | The Darkness II Lua scripts, QuickBMS extraction pipeline | **Reference Only** (proprietary assets; confirms runtime hook need) |

> [!IMPORTANT]
> **Boundary Notice:** Diagnostic tools and pipeline inspectors can analyze matrices, draw calls, and shaders, but **no tool can repair FOV, culling, or stereo merely because it can inspect the rendering pipeline**. Repairs require correct matrix transformations, culling plane synchronization, and camera state management in the runtime proxy.

---

## 2. Priority A — apitrace-mcp & apitrace

### 2.1 Overview & Architecture
`apitrace-mcp` is a specialized Model Context Protocol (MCP) server wrapping upstream `apitrace`. Unlike generic CLI wrappers, it incorporates domain-specific geometry algorithms tailored for reverse-engineering older games (DirectDraw, D3D7, D3D8, D3D9, legacy OpenGL).

### 2.2 System & Runtime Prerequisites
- **Python Version:** Python `>= 3.11` (Tested clean under Python 3.14.5).
- **Core Dependencies:** `mcp>=1.29,<2`.
- **Optional Dependencies:** `Pillow>=10,<13` (only required for image comparison sidecars / `diff_images`).
- **Standard Library:** Core matrix classification, PE header inspection, worker process management, and pickle decoding operate entirely on Python standard library modules (`math`, `re`, `subprocess`, `pickle`, `json`, `dataclasses`).

### 2.3 32-Bit apitrace Requirements for The Darkness II
- The target executable `DarknessII.exe` is a **32-bit x86 binary** (`IMAGE_FILE_MACHINE_I386`).
- **Strict Rule:** A 64-bit apitrace (`win64`) **cannot** trace a 32-bit process. The wrapper DLL (`d3d9.dll`) must be loaded directly into the target process's virtual address space.
- **Required Package:** Upstream `apitrace-*-win32.7z`.
- **Key Files Required:**
  - `lib/wrappers/d3d9.dll` (32-bit wrapper)
  - `bin/apitrace.exe` (32-bit CLI)
  - `bin/d3dretrace.exe` (32-bit Direct3D replayer)
- **Auto-Discovery:** `apitrace-mcp` automatically detects executable bitness via PE header analysis (`pe.py:read_pe`) and selects the 32-bit root if configured via `APITRACE_MCP_ROOT_WIN32` or present in search paths (`C:\apitrace\apitrace-*-win32`).

### 2.4 Diagnostic Capabilities Verified
- **Matrix Classification:** Scans driver uploads (`SetVertexShaderConstantF`, `SetTransform`), isolates candidate 4x4 windows, and derives view-frustum bounds.
- **Camera Tracking:** Derives camera eye position and forward view direction across draw streams.
- **Shader Listing & Device State:** Extracts bytecode dumps and queries full device JSON snapshots via `d3dretrace.exe --dump-state`.

### 2.5 Analysis of Existing Darkness2VR Traces
- **Format Distinction:** Darkness2VR's existing trace files (`test-output/run_*_trace.txt`, `test-output/engine-trace-live.csv`) are custom ASCII logs emitted by `stereo_replay.inl` / `render_pipeline_trace.inl`. They are **not** binary apitrace `.trace` files.
- `apitrace.exe` cannot read Darkness2VR's ASCII logs.
- The mathematical principles of frustum plane extraction apply directly to Darkness2VR constant uploads when correlated with pass and shader provenance.

### 2.6 Preventing Conflicts with Darkness2VR Proxy and Steam Overlay

```
  [Normal Launch]
  DarknessII.exe  -->  GameOverlayRenderer.dll (Steam)  -->  d3d9.dll (Darkness2VR Proxy)  -->  C:\Windows\SysWOW64\d3d9.dll

  [Conflict State (Accidental Overwrite)]
  DarknessII.exe  -->  d3d9.dll (Apitrace Wrapper)  [Darkness2VR proxy overwritten! Mod inactive]

  [Recommended Safe Baseline Capture]
  DarknessII.exe  -->  d3d9.dll (Apitrace 32-bit)  -->  C:\Windows\SysWOW64\d3d9.dll
  (Steam Overlay disabled; Darkness2VR proxy temporarily backed up)
```

**Conflict Prevention Rules for MAIN:**
1. **Never drop apitrace `d3d9.dll` into the game directory while Darkness2VR's `d3d9.dll` is installed.** Both use the identical filename.
2. **Disable the Steam Overlay:** In Steam, right-click *The Darkness II* -> Properties -> General -> uncheck *"Enable the Steam Overlay while in-game"*. `GameOverlayRenderer.dll` hooks the same Direct3D 9 COM vtable and causes hook collisions or crash-on-exit with apitrace.
3. **Capture Workflow:**
   - Step 1: Back up Darkness2VR's `d3d9.dll` (e.g., rename to `d3d9.dll.d2vr`).
   - Step 2: Copy 32-bit apitrace `lib/wrappers/d3d9.dll` into the game directory.
   - Step 3: Run `DarknessII.exe` via Steam for 10–20 seconds (menu to gameplay transition).
   - Step 4: Exit game cleanly. Apitrace writes `DarknessII.trace` to the game folder or configured trace path.
   - Step 5: Remove apitrace's `d3d9.dll` and restore `d3d9.dll.d2vr`.

---

## 3. Priority B — wiz3D Evaluation & Comparison

### 3.1 Overview
`wiz3D` is the modern community continuation of the iZ3D stereoscopic 3D driver. It contains an extensive Direct3D 9 proxy layer (`S3DWrapper9`) handling automatic stereo projection, shader conversion, and profile-based shader overrides.

### 3.2 Key Implementations in wiz3D (`S3DWrapper9`)

#### 1. ShaderOverride Loader (`ShaderOverrideLoader.cpp`)
- Replaces shaders by CRC32 filename: `<game>\ShaderOverride\<VertexShaders|PixelShaders>\<CRC32_HEX>.txt`.
- Reads assembly text and invokes `D3DXAssembleShader`. If successful, the assembled bytecode replaces the original shader at `CreateVertexShader`/`CreatePixelShader` time.
- If assembly fails, logs errors and falls back to original bytecode.

#### 2. Shader CRC Matching (`CalculateCRCEx`, `System.cpp`)
- Computes standard IEEE 802.3 32-bit CRC over raw shader bytecode.
- Implements `bSkipComments`: strips `D3DSIO_COMMENT` tokens before calculating CRC32, ensuring identical CRC across executions even if compiler timestamps vary.

#### 3. Vertex/Pixel Shader Dump & Hunter Tooling (`ShaderHunter9.cpp`)
- Provides an interactive in-game hunter mode:
  - `Ctrl + F2`: Toggle hunter mode.
  - `F2` / `Shift + F2`: Cycle through active shaders drawn in the current frame.
  - `HunterShouldSkipDraw`: Dynamically skips draw calls using the currently selected shader, making objects disappear in real time on screen.
  - `F3`: Dumps disassembled SM3 bytecode via `D3DXDisassembleShader` into `wiz3D_Logs/<Game>/Shaders/Hunter/<CRC>_<stage>_<ts>.txt`.

#### 4. Detection of Shaders with Existing Stereo Displacement (`ScanForStereoTexldl`)
- Scans bytecode tokens for opcode `D3DSIO_TEXLDL` bound to sampler register 0 (`texldl r0, c210, s0`).
- If detected, `m_CurrentShaderData.hasStereoTexldl = true`. The driver skips its own matrix adjustment for this draw call to avoid **double-shifting**.

#### 5. Per-Shader Stereo Exclusions
- XML profile configuration `<Mono>` or `m_Multiplier == 0.0f` sets `shaderIsMono = true`.
- Shaders without 3D perspective transforms are classified `AnalysisSharedIsMono` and rendered only once without stereo displacement.

### 3.3 Comparison: wiz3D vs Darkness2VR (`src/stereo_replay.inl`)

| Feature | wiz3D (`S3DWrapper9`) | Darkness2VR (`src/stereo_replay.inl`) | Assessment for Darkness2VR |
|---|---|---|---|
| **Stereo Architecture** | Dual draw / state duplication at driver level | Same-frame record and replay (eye 0 live, eye 1 replay) | Darkness2VR approach is superior for OpenXR VR headset frame submission |
| **Matrix Identification** | Dynamic AST tree analysis (`ShaderAnalyzer.cpp`) | Opcode token stream scan (`AnalyzeVsTokenStreamInner`) + CTAB reflection | Darkness2VR has faster bytecode parsing; wiz3D has broader heuristics |
| **Shader Hashing** | IEEE 802.3 CRC32 (with comment stripping) | FNV-1a 32-bit hash (`HashBytes`) | **Gap:** Darkness2VR cannot drop in community HelixMod `.txt` fixes directly by filename (they use CRC32) |
| **Shader Override** | File-based runtime assembler (`ShaderOverrideLoader`) | Hardcoded C++ inline patches | **Valuable addition:** Adding a CRC32 file override loader would allow live tuning of shadow/particle shaders |
| **HelixMod Collision Detection** | `ScanForStereoTexldl` checks for `texldl ... s0` | Not implemented | **Valuable addition:** Prevents double-displacement if HelixMod fixes are used |
| **Live Shader Hunting** | Interactive draw skipping (`HunterShouldSkipDraw`) + F2/F3 hotkeys | Offline frame log dump (`run_*_trace.txt`) | **Valuable addition:** Live shader hunting greatly speeds up isolating rogue lighting or shadow shaders |

---

## 4. Priority C — LotusLib, Warframe-Exporter & Engine Decompilation

### 4.1 Evaluation of LotusLib (`Puxtril/LotusLib`)
- **TOC Archive Format:** LotusLib targets Warframe's `Cache.Windows` `.toc` / `.cache` files.
- **Binary Layout Verification:** `RawTOCEntry` in `LotusLib/include/RawEntry.h` is 96 bytes (`0x60`), matching the entry size and field layout of The Darkness II's QuickBMS script (`tools/darkness2.bms`):
  ```cpp
  struct RawTOCEntry {
      int64_t cacheOffset;    // 8 bytes (0xFFFFFFFFFFFFFFFF = directory)
      int64_t timeStamp;      // 8 bytes (0 = deleted/duplicate)
      int32_t compressedLen;  // 4 bytes
      int32_t length;         // 4 bytes
      int32_t reserved;       // 4 bytes
      int32_t parentDirIndex; // 4 bytes
      char name[64];          // 64 bytes null-terminated name
  }; // Total: 96 bytes (0x60)
  ```
- **Correction on Tested Capabilities:** While binary layout compatibility of the TOC structure was verified against `tools/darkness2.bms`, **real The Darkness II archive parsing with LotusLib remains untested in this environment**, as no end-to-end read test was executed on game archives. Real extraction is currently verified exclusively via QuickBMS `darkness2.bms`.

### 4.2 Distinguishing Engine Capabilities (Mandatory Differentiation)

| Capability | LotusLib Status | Warframe-Exporter Status | The Darkness II Status |
|---|---|---|---|
| **Reading Archives** | Supported (reads `.toc` hierarchy, file offsets, sizes) | Uses LotusLib | Supported via `darkness2.bms` in QuickBMS |
| **Decompression** | Implements `decompressEE` (LZF and Oodle), but requires external proprietary Oodle SDK to link | Uses LotusLib | The Darkness II primarily uses Evolution LZF (`comtype evolution`), fully handled by QuickBMS |
| **Decoding Materials/Textures/Meshes/Levels** | **NOT SUPPORTED** (LotusLib is strictly a container reader; explicitly has zero asset knowledge) | Supported for Warframe formats via ImHex patterns and custom parsers | Formats differ across engine generations (2012 D2 vs modern Warframe) |
| **Repacking Modified Game Assets** | **NOT SUPPORTED** | **NOT SUPPORTED** | **NO PUBLIC TOOL EXISTS** |

### 4.3 Review of Warframe-Exporter ImHex Patterns (`util/*.hexpat`)
- `wf_common-header.hexpat`: Decodes Evolution Engine asset headers (16B MD5, path strings, properties, type enum).
- `wf_shader21.hexpat` / `wf_shader22.hexpat`: Decodes bytecode lengths and SM3 shader chunks.
- **Takeaway:** ImHex patterns provide clean format specifications for inspecting raw game files in a hex editor without writing custom C++ decoders.

### 4.4 The Darkness II Decompiled (`FromDarkHell/TheDarknessIIDecompiled`)
- Contains decompiled Lua scripts and Flash ActionScript UI components.
- Confirms that gameplay logic, weapon scripts, and UI bindings are Lua-driven, while the rendering engine, culling, and camera transformations reside inside `DarknessII.exe`.

> [!CAUTION]
> **Definitive Finding:** Attempting to mod The Darkness II for VR by repacking `.cache` and `.toc` files on disk is impractical, fragile, and unproven. Runtime memory hooks and D3D9 proxy injection (Darkness2VR's current architecture) are the only reliable mechanisms for modifying projections, camera matrices, and stereo replay.

---

## 5. Matrix Adapter Accuracy Gate & Reconciliation

### 5.1 Analysis of Prior Adapter Inaccuracies
Our initial matrix inspection prototype exhibited three critical failure modes that invalidated its output as authoritative evidence:
1. **Stale Shader Constants:** Constants uploaded during early setup passes persisted in memory and were evaluated against draw calls in later, unrelated passes.
2. **Sliding Window False Positives in Bone Palettes:** A skeletal bone palette upload of 165 constants (`op=20 a=45,165`) was naively subdivided into 4-register sliding windows (`c45..c48`, `c47..c50`, `c49..c52`...). The adapter evaluated these sub-windows and reported spurious $90^\circ$ "shadow viewproj" matrices across dozens of draws.
3. **Missing Shader & Pass Context:** Draw calls were evaluated without checking whether the active vertex shader actually reads the constant registers, and without checking the active render target format.

### 5.2 Mathematical Reconciliation: Apparent FOV (`119.21° × 112.21°`) vs Camera Diagonal (`60.0°`)
The initial adapter reported a view-projection matrix at `c0..c3` with an apparent FOV of `119.21° × 112.21°`. This appeared in conflict with the documented camera projection coefficients ($P_x = 1.768088, P_y = 1.732051$, yielding $60.00^\circ$ vertical FOV).

**The Mathematical Cause:**
In the Evolution Engine, `c0..c3` does **not** store pure camera view-projection ($V \cdot P$); it stores the combined per-object world-view-projection:
$$M = W \cdot V \cdot P$$
where $W$ is the local object's model transform (`c4..c7` in the `LitInstance` struct).
- Gribb-Hartmann frustum plane extraction derives the viewing volume boundary planes in the **coordinate space of the input vertices** (i.e., in local model space).
- If the object has a non-identity model transform $W$ with non-uniform scaling or local rotation (e.g. character heads, tentacles, weapons), the angular span between opposing plane normals $\hat{n}_l$ and $\hat{n}_r$:
  $$\cos(\theta) = \hat{n}_l \cdot \hat{n}_r$$
  reflects the **object-space apparent frustum angle**, not the pure camera view-space FOV!
- When $W = I$ (static world geometry, such as command 227 where $W$ is identity), $M = V \cdot P$. Calculating the column ratios on command 227 yields:
  $$P_x = \frac{\|\text{col}_0\|}{\|\text{col}_3\|} = 1.768089, \quad P_y = \frac{\|\text{col}_1\|}{\|\text{col}_3\|} = 1.732051$$
  $$2 \arctan\left(\frac{1}{P_y}\right) = 60.0000^\circ, \quad 2 \arctan\left(\frac{1}{P_x}\right) = 58.9835^\circ$$
- Furthermore, command 2431 in pixel shader uploads the `InverseProjection` matrix to `c27..c30`:
  $$\text{invP}_{00} = 0.5655826 = \frac{1}{P_x}, \quad \text{invP}_{11} = 0.5773503 = \frac{1}{P_y}$$
  $$2 \arctan(0.5773503) = 60.0000^\circ, \quad 2 \arctan(0.5655826) = 58.9835^\circ$$

**Conclusion:** Both measurements are mathematically consistent when coordinate spaces are properly identified:
- `60.0°` is the **camera view-space projection coefficient** (derived from PS `c27..c30` and pure VP `c0..c3` at command 227).
- `119.21° × 112.21°` is the **object-space apparent plane angle** of a scaled local mesh uploaded to `c0..c3` at a specific draw call.

### 5.3 Categorization of Matrix Types & Register Independence
> [!CAUTION]
> **Cardinal Rule:** Register identity alone (`c0..c3`, `c20..c23`) **never** proves a rendering pass's purpose. Pass purpose requires correlating render target dimensions, pixel formats, vertex shader bytecode outputs, and upload provenance.

1. **Projection-Consistent WVP (`PROJECTION_CONSISTENT_WVP`):**
   - Active render target is the main HDR scene target (`3076x3140 format=113`) or forward pass (`format=20/21/22`).
   - Active vertex shader drives clip position `o0` from the slot (`guessed=0` or `named=0`).
   - Projection scales match $P_x \approx 1.768, P_y \approx 1.732$ ($60.0^\circ$ FOV).
   - **Identity Limitation:** Even when World=Identity ($W = I$) is witnessed (e.g. adjacent `LitInstance` model matrix `c4..c7` is identity), static analysis cannot establish active player camera ownership without a runtime witness, as auxiliary cameras or reflection passes can share identical projection coefficients.
2. **Correlated Object WVP (`CORRELATED_OBJECT_WVP`):**
   - Active in 3D geometry passes, driving clip position, but carrying a verified non-identity model transform $W$ (`c4..c7`). Plane angles describe object-space boundaries.
3. **Shadow Pass WVP (`SHADOW_PASS_WVP`):**
   - Active render target is the shadow map atlas (`4096x4096 format=23 D3DFMT_R5G6B5` or `format=114 D3DFMT_R32F`).
   - Used for depth/shadow rendering from light sources (directional cascade or cubemap).
4. **Screen Texture Projection (`SCREEN_TEX_PROJ`):**
   - Active vertex shader drives an auxiliary texture coordinate output (e.g. `guessed_stp=4` or `stpBase=20`), used for decals, shadow projection, and deferred lighting lookups.
5. **Model Transform Affine (`MODEL_RIGID_AFFINE`):**
   - 4x4 or 4x3 rigid transform with orthogonal rotation rows ($l \approx 1.0$) and translation row (e.g. `c4..c7` LitInstance).
6. **Orthographic UI (`ORTHO_UI`):**
   - 2D orthographic projection matrix with zero perspective terms and identity $m[15] = 1.0$.
7. **Skeletal Bone Palette (`BONE_PALETTE`):**
   - Massive uploads with $\text{count} > 16$ uploaded to registers $\ge c32$ (e.g. `c45..c209`). Quarantined so that sliding 4x4 windows within bone palettes are never evaluated as camera candidates. Bulk uploads at $c0$ ($\text{start} < 32$) are distinguished and not quarantined.
8. **D3D9 Constant Persistence & Bank Isolation:**
   - Constants persist across `SetRenderTarget` calls in D3D9 until overwritten, reset, or stateblock restored. Constants are never falsely rejected across passes; upload pass and draw pass are reported separately.
   - VS constants (`op=20`) and PS constants (`op=27`) are maintained in strictly separate banks to prevent cross-bank contamination.

---

## 6. Grounded Evidence Levels & Test Verification

The updated adapter replaces arbitrary confidence numbers with explicit grounded evidence levels:

```
EvidenceLevel:
  - PROJECTION_CONSISTENT_WVP (Px/Py match camera coefficients; unproven active camera identity)
  - CORRELATED_OBJECT_WVP   (Pass and shader match, but carries verified non-identity model transform W)
  - SHADOW_PASS_WVP         (Verified shadow atlas pass: 4096x4096 R5G6B5 / R32F)
  - SCREEN_TEX_PROJ         (VS bytecode confirms texture coordinate output)
  - MODEL_RIGID_AFFINE      (Verified rigid 4x4 rotation + translation)
  - ORTHO_UI                (Verified 2D orthographic matrix)
  - BONE_PALETTE            (Skeletal palette block; quarantined from camera evaluation)
  - UNVERIFIED_CANDIDATE    (Valid 4x4 math, but missing active shader correlation)
  - UNKNOWN                 (Unrecognized or degenerate matrix)
```

### 6.1 Clean-Room Provenance & License Compliance
To strictly comply with open-source licensing rules, **no code was copied from `apitrace-mcp`**. The local adapter ([`tools/external-toolchain/trace_matrix_adapter.py`](file:///C:/Users/Trilex/.codex/worktrees/external-toolchain/Darkness2VR/tools/external-toolchain/trace_matrix_adapter.py)) was independently authored clean-room using standard published algorithms:
- Gribb & Hartmann (2001) for frustum plane extraction.
- Microsoft Direct3D 9 pipeline documentation for row-major matrix transformations.

### 6.2 Deterministic Unit Test Suite
We created and verified a dedicated portable unit test suite:
[`tools/external-toolchain/test_trace_matrix_adapter.py`](file:///C:/Users/Trilex/.codex/worktrees/external-toolchain/Darkness2VR/tools/external-toolchain/test_trace_matrix_adapter.py)

**Test Suite Coverage (17 Deterministic Tests):**
1. `test_synthetic_camera_projection`: Positive test validating FOV recovery on pure $60^\circ$ D3D9 perspective projection.
2. `test_transposed_perspective_coefficients`: Validates that transposed (column-major) matrices are normalized consistently to row-vector layout, computing exact identical $P_x, P_y$ scales.
3. `test_synthetic_view_projection_apex`: Positive test recovering exact camera eye position $(10.0, 20.0, 30.0)$ from View-Projection matrix.
4. `test_vs_ps_constant_bank_separation`: Defect P1 regression: ensures `op=27` PS constant uploads never contaminate `vs_constants`.
5. `test_persistent_vs_constants_across_rt0_changes`: Defect P1 regression: ensures valid D3D9 constants persist across `SetRenderTarget` calls without false `STALE_CROSS_PASS` rejection.
6. `test_same_size_different_rt0_identities`: Defect P1 regression: tracks RT0 identity by surface pointer and viewport, distinguishing same-size render targets.
7. `test_unknown_vs_binding_reset`: Defect P1 regression: ensures `op=19` without a following `VS` metadata line conservatively clears active VS bindings.
8. `test_state_block_restore`: Defect P1 regression: ensures `op=36 ApplyStateBlock` conservatively invalidates active shader bindings.
9. `test_missing_bulk_payload`: Defect P1 regression: ensures bulk uploads where trace omits `cN` lines mark affected registers unknown.
10. `test_partial_uploads`: Defect P1 regression: ensures partial constant uploads ($< 4$ registers) cannot be evaluated as 4x4 matrices.
11. `test_ambiguous_transpose`: Defect P2 regression: flags `layout="ambiguous"` when both row-major and transposed forms solve.
12. `test_auxiliary_camera_downgraded_to_projection_consistency`: Defect P2 regression: downgrades unproven camera identity to `PROJECTION_CONSISTENT_WVP`.
13. `test_true_bone_palette_versus_bulk_wvp`: Defect P2 regression: distinguishes bulk uploads at $c0$ ($\text{start} < 32$) from skeletal bone palettes ($\text{start} \ge 32$).
14. `test_d3dfmt_constants_correctness`: Defect P2 regression: verifies format 23 is `D3DFMT_R5G6B5` and format 114 is `D3DFMT_R32F`.
15. `test_rt_switching_and_mrt_isolation`: Verifies that pass determination is governed strictly by RT0 (not auxiliary MRTs) and that RT0 unbinding resets pass to `UNKNOWN_PASS`.
16. `test_deterministic_synthetic_trace_fixture`: Self-contained multi-pass synthetic trace fixture verifying full pipeline parsing on any machine.
17. `test_real_darkness2_trace_reconciliation`: Regression test against real Darkness II trace (`run_43052_frame_003_trace.txt`) with explicit `SKIPPED` handling when the local trace file is absent.

**Test Execution:**
```powershell
python tools/external-toolchain/test_trace_matrix_adapter.py
```
**Result:** `Ran 17 tests in 0.032s - OK (Total=17 | Passed=17 | Failed=0 | Skipped=0 on local fixture; portable skip on remote machines)`.

---

## 7. Verified Output on Real Game Trace (`run_43052_frame_003_trace.txt`)

Running the validated adapter on the real frame trace:
```powershell
python tools/external-toolchain/trace_matrix_adapter.py C:\Users\Trilex\Darkness2VR\test-output\run_43052_frame_003_trace.txt
```

**Output:**
```text
=========================================================================================================
 DARKNESS2VR PROVENANCE-AWARE MATRIX ANALYSIS
=========================================================================================================
Slot       Evidence Level               Pass Context              FOV(H x V)       Camera Eye (X, Y, Z)     
---------------------------------------------------------------------------------------------------------
c0..c3     CORRELATED_OBJECT_WVP        MAIN_FORWARD              58.98 x 60.0     (-1.15, 1.8, -17.93)     
  --> Note: Combined object WVP carrying non-identity model transform W (LitInstance c4..c7); plane angles are object-space [Uploaded at cmd 2845 in MAIN_FORWARD, drawn at cmd 2848 in MAIN_FORWARD] (Observed in 41 draws)
c0..c3     CORRELATED_OBJECT_WVP        MAIN_HDR_SCENE            58.98 x 60.0     (-2.96, 1.07, -11.88)    
  --> Note: Combined object WVP carrying non-identity model transform W (LitInstance c4..c7); plane angles are object-space [Uploaded at cmd 3080 in MAIN_HDR_SCENE, drawn at cmd 3087 in MAIN_HDR_SCENE] (Observed in 93 draws)
c0..c3     PROJECTION_CONSISTENT_WVP    MAIN_FORWARD              58.98 x 60.0     (0.0, 0.02, 0.0)         
  --> Note: Projection scales match camera coefficients (Px=1.768, Py=1.732), but World transform is unverified [Uploaded at cmd 3618 in MAIN_FORWARD, drawn at cmd 3625 in MAIN_FORWARD] (Observed in 12 draws)
c0..c3     PROJECTION_CONSISTENT_WVP    MAIN_HDR_SCENE            58.98 x 60.0     (61.62, 1.82, -25.73)    
  --> Note: Projection scales match camera coefficients (Px=1.768, Py=1.732), but World transform is unverified [Uploaded at cmd 3117 in MAIN_HDR_SCENE, drawn at cmd 3124 in MAIN_HDR_SCENE] (Observed in 17 draws)
c0..c3     SHADOW_PASS_WVP              SHADOW_ATLAS              79.34 x 128.42   (-0.16, 1.32, -1.43)     
  --> Note: Shadow depth pass (4096x4096 R5G6B5 atlas) [Uploaded at cmd 36 in SHADOW_ATLAS, drawn at cmd 37 in SHADOW_ATLAS] (Observed in 1 draws)
c0..c3     UNVERIFIED_CANDIDATE         POSTFX_DOWNSAMPLE         58.98 x 60.0     (61.62, 1.82, -25.73)    
  --> Note: Uploaded in pass POSTFX_DOWNSAMPLE [Uploaded at cmd 3416 in POSTFX_DOWNSAMPLE, drawn at cmd 3434 in POSTFX_DOWNSAMPLE] (Observed in 2 draws)
c4..c7     SCREEN_TEX_PROJ              MAIN_HDR_SCENE            - x -            -                        
  --> Note: VS designated ScreenTextureProjection slot (guessed/named 4) [Uploaded at cmd 2457] (Observed in 5 draws)
=========================================================================================================
```

### Key Differences from Initial Prototype:
1. **Zero Bone Palette False Positives:** The 58 false sliding windows inside `c45..c209` are quarantined.
2. **Clear Pass Discrimination:** Shadow atlas depth passes ($4096\times 4096$ R5G6B5) are explicitly separated from main scene passes.
3. **Camera Projection Consistency Separated from Identity:** Matrices matching $P_x = 1.768, P_y = 1.732$ ($60.0^\circ$ FOV) are reported as `PROJECTION_CONSISTENT_WVP`; camera identity is never overclaimed.
4. **Apparent Object FOVs Clarified:** Local object-space viewing volume bounds carrying model transforms $W$ are distinguished.
5. **Exact Upload Command Recorded:** Every classification reports the exact upload command index and upload pass (`[Uploaded at cmd ...]`).

---

## 8. Instructions for MAIN

To inspect any new or existing record-stream trace:

```powershell
# 1. Run the provenance-aware matrix trace analyzer
python tools/external-toolchain/trace_matrix_adapter.py test-output/run_<pid>_frame_<num>_trace.txt

# 2. Run the deterministic portable test suite
python tools/external-toolchain/test_trace_matrix_adapter.py -v
```


## 9. MAIN acceptance - 2026-10-24

Accepted on `codex/integration-frame-capture`, preserving existing Luna/Sol/Astra
and matched-capture work, from the research/evolution base (never main).
Complete Gemini series, each absent by ancestry/patch comparison before picking:
`ae8de22 -> 5ae340f`, `0f9085f -> f524542`, `85be172 -> ce6383a`,
`120b9e0 -> 029821b`. The historical results above describe the Gemini tip;
the following additional gates were applied before MAIN acceptance.

### SetViewport / op=7: UNKNOWN when payload is absent

Verified against `src/stereo_replay.inl::Hook_SetViewport`: a[0] is the offset
returned by RecPush for a D3DVIEWPORT9 in the private record arena. Replay reads
that arena at a[0]. The legacy text exporter in
`src/d3d9.cpp::DumpEyeFramesIfRequested` emits the numeric op arguments but no
viewport payload or arena bytes. Actual viewport recovery from this text is
therefore NOT POSSIBLE, including when the offset happens to be zero.

The adapter now invalidates rt0_viewport at every op=7, exposes None on the
classification and prints viewport=UNKNOWN in its summary. MRT attachments do
not recover it. A later RT0 binding with dimensions restores the full-target
viewport as the D3D9 SetRenderTarget reset, under the existing trace assumption
that recorded calls succeeded (HRESULTs are not exported). Grouping includes
RT0 identity and viewport, so an UNKNOWN observation cannot be hidden behind a
same-size representative. RT attachment width/height remain independent facts.
No production trace-writer changes were made to invent missing payloads.

### ApplyStateBlock / op=36: forget unknown restored state

The trace exports only a stateblock pointer, not its captured register contents
or capture mask. Clearing shader bindings alone was insufficient: rebind of the
same VS could reuse pre-Apply constants, and partial post-Apply uploads could
combine new rows with unknown restored rows.

The adapter now clears both VS/PS float banks, both upload-command/pass maps,
bone-palette quarantine, pending upload context and viewport, as well as the
previously cleared shader bindings. Only explicit post-Apply uploads can recover
register knowledge, independently per bank. This deliberately loses knowledge
when a stateblock might preserve a subset; it never promotes old values to
current authority. Ordinary RT switches still preserve D3D9 constants.

Eight new regression methods cover opaque viewport offsets (0/24/4096), visible
UNKNOWN summary, MRT isolation, RT0 reset, shader rebind after Apply, partial
uploads, independent fresh-bank recovery and orphan upload/palette invalidation.
They exposed failures on the unmodified candidate before the fix.
A small additional enum correction was verified directly in local Windows SDK
10.0.26100.0/shared/d3d9types.h: format 50 is L8, D16 is 80. Both are asserted in
the existing format regression test.

### Final results and limits

- Adapter: 25/25 PASS, 0 failures, 0 skips. This includes the available private
  `run_43052_frame_003_trace.txt` replay; no private trace is committed.
- Existing matched-capture validator: 10/10 PASS.
- Existing executable-audit parsers: 6/6 PASS.
- Luna image/ray analysis self-test: PASS.
- Production scope check: zero diff versus 9c9496f in src/, build.ps1 and
  Darkness2VR.ini. Original checkout's untracked MODLOG remains unchanged.
- No DLL deployment, native-FOV change, Steam installation change or game launch.

Adapter acceptance is DIAGNOSTIC ONLY. Coefficient agreement, identity-model
matrices, recovered apex and render-target heuristics do not prove an active
player camera, native CPU frustum ownership or same-frame source/final images.
The next work remains the separately planned matched RT0/eye capture and native
camera-to-visibility chain, with its existing AV/presentation runtime gates.
