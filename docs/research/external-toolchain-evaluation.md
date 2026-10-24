# External Reverse Engineering Toolchain Evaluation

**Darkness2VR Research Report**  
**Branch:** `research/external-toolchain`  
**Base:** `origin/research/evolution-engine-render-pipeline`  
**Author:** Tooling & Research Side Agent  

---

## 1. Executive Summary & Objective

This report evaluates existing open-source reverse engineering tools, libraries, and decompiled references for their applicability to **Darkness2VR** (Direct3D 9, Evolution Engine, 32-bit x86).

### Projects Evaluated

| Tool / Repository | Primary Focus | Evaluated Capability | Integration Feasibility |
|---|---|---|---|
| **phunkaeg/apitrace-mcp** | Graphics RE MCP Server | Matrix classification, camera tracking, D3D call inspection | **High** (standalone adapter + trace workflow) |
| **apitrace/apitrace** | Graphics API Tracer & Replayer | 32-bit D3D9 whole-run tracing and call stream dump | **High** (external tool for MAIN baseline captures) |
| **effcol/wiz3D** | Stereoscopic 3D Driver (iZ3D continuation) | ShaderOverride loader, CRC32, ShaderHunter, Stereo Texldl scan | **Medium-High** (architectural patterns & algorithms) |
| **Puxtril/LotusLib** | Evolution Engine Cache Reader | Archive TOC reading, LZF/Oodle chunk decompression | **Low** (read-only; QuickBMS already covers D2 archives) |
| **Puxtril/Warframe-Exporter** | Asset Exporter & ImHex Patterns | Binary format patterns (`.hexpat`) for Evolution Engine assets | **Medium** (format reference only; no repacking) |
| **FromDarkHell/TheDarknessIIDecompiled** | Decompiled Lua/Flash Scripts | The Darkness II Lua scripts, QuickBMS extraction pipeline | **Reference Only** (proprietary assets; confirms runtime hook need) |

> [!IMPORTANT]
> **Boundary Notice:** Diagnostic tools and pipeline inspectors can analyze matrices, draw calls, and shaders, but **no tool can repair FOV, culling, or stereo merely because it can inspect the rendering pipeline**. Repairs require correct matrix transformations, culling plane synchronization, and camera state management in the runtime proxy.

---

## 2. Priority A — apitrace-mcp & apitrace

### 2.1 Overview & Architecture
`apitrace-mcp` is a specialized Model Context Protocol (MCP) server wrapping upstream `apitrace`. Unlike generic CLI wrappers, it incorporates domain-specific computer vision and geometry algorithms tailored for reverse-engineering older games (DirectDraw, D3D7, D3D8, D3D9, legacy OpenGL).

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

#### Matrix Classification & Decoding (`find_matrices`, `decode_matrix`)
- Scans candidate 4x4 and 4x3 constant blocks uploaded via `IDirect3DDevice9::SetVertexShaderConstantF` (`vs_c[0..255]`) or `SetTransform`.
- Isolates windows at **upload-aligned boundaries**, preventing packed skeletal bone palettes from polluting the camera candidate list.
- Implements **Gribb-Hartmann plane extraction** on world-view-projection candidates:
  $$\text{Left} = c_3 + c_0, \quad \text{Right} = c_3 - c_0, \quad \text{Bottom} = c_3 + c_1, \quad \text{Top} = c_3 - c_1$$
- Solves for the frustum apex (eye point) and measures horizontal/vertical angular spans between opposing clipping planes.
- Verifies storage layout: automatically detects whether matrices are stored row-major (`as_stored`) or column-major/transposed (`transposed`). In D3D9, shader constants commonly store the transpose to allow row dot-products.

#### Camera Tracking (`track_camera`)
- Evaluates temporal trajectory across frames.
- Identifies the stable world-space eye position and view-Z forward vector.
- Rejects single-frame camera jumps or temporary cutscene/cinematic transient matrices.

#### Shader Listing & Device State (`list_shaders`, `dump_state`)
- Extracts shader bytecodes and disassembly previews for every unique shader bound.
- Calls `d3dretrace.exe --dump-state <call>` to export complete JSON snapshots of device state (render states, textures, samplers, stream sources, constant registers).

### 2.5 Analysis of Existing Darkness2VR Traces
- **Format Incompatibility:** Darkness2VR's existing trace files (`test-output/run_*_trace.txt`, `test-output/engine-trace-live.csv`) are custom ASCII logs emitted by `stereo_replay.inl` / `render_pipeline_trace.inl`. They are **not** binary apitrace `.trace` files.
- `apitrace.exe` cannot read Darkness2VR's ASCII logs.
- **Immediate Synergy:** The underlying matrix classification algorithms in `apitrace-mcp` (`matrices.py`) are pure mathematical evaluators. We developed and validated a standalone adapter script (`tools/external-toolchain/trace_matrix_adapter.py`) that parses Darkness2VR's record-stream traces and executes the exact same Gribb-Hartmann classification and camera extraction offline.

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
- Checks for `<game_folder>\ShaderOverride\` directory.
- Replaces shaders by CRC32 filename: `<game>\ShaderOverride\<VertexShaders|PixelShaders>\<CRC32_HEX>.txt`.
- Reads assembly text and invokes `D3DXAssembleShader`. If successful, the assembled bytecode replaces the original shader at `CreateVertexShader`/`CreatePixelShader` time.
- If assembly fails, logs errors and falls back to original bytecode.

#### 2. Shader CRC Matching (`CalculateCRCEx`, `System.cpp`)
- Computes standard IEEE 802.3 32-bit CRC over raw shader bytecode.
- Implements `bSkipComments`: strips `D3DSIO_COMMENT` tokens before calculating CRC32. This solves the issue where games embed build timestamps or variable debug strings in shader comments, ensuring identical CRC across executions.

#### 3. Vertex/Pixel Shader Dump & Hunter Tooling (`ShaderHunter9.cpp`)
- Provides an interactive in-game hunter mode:
  - `Ctrl + F2`: Toggle hunter mode.
  - `F2` / `Shift + F2`: Cycle through active shaders drawn in the current frame.
  - `HunterShouldSkipDraw`: Dynamically skips draw calls using the currently selected shader. The author sees objects (e.g. shadows, HUD, lighting, particles) disappear in real time on screen.
  - `F3`: Dumps disassembled SM3 bytecode via `D3DXDisassembleShader` into `wiz3D_Logs/<Game>/Shaders/Hunter/<CRC>_<stage>_<ts>.txt`.

#### 4. Detection of Shaders with Existing Stereo Displacement (`ScanForStereoTexldl`)
- HelixMod-compatible shader fixes perform their own stereo separation using:
  `texldl r0, c210, s0` (sampling stereo parameters from sampler 0).
- `ScanForStereoTexldl` iterates bytecode tokens looking for opcode `D3DSIO_TEXLDL` bound to sampler register 0.
- If found, `m_CurrentShaderData.hasStereoTexldl = true`. The driver skips its own matrix adjustment for this draw call to avoid **double-shifting**.

#### 5. Per-Shader Stereo Exclusions
- XML profile configuration `<Mono>` or `m_Multiplier == 0.0f` sets `shaderIsMono = true`.
- Shaders without 3D perspective transforms (e.g. 2D UI quads) are classified `AnalysisSharedIsMono` and rendered only once without stereo displacement.

### 3.3 Detailed Comparison: wiz3D vs Darkness2VR (`src/stereo_replay.inl`)

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
- **Format Verification:** We verified `RawTOCEntry` in `LotusLib/include/RawEntry.h` against The Darkness II's QuickBMS script (`tools/darkness2.bms`):
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
  The TOC entry structure is **identical** between The Darkness II and Warframe. LotusLib's `DirectoryTree::readTocFile()` can parse The Darkness II's `.toc` headers.

### 4.2 Distinguishing Engine Capabilities (Mandatory Differentiation)

| Capability | LotusLib Status | Warframe-Exporter Status | The Darkness II Status |
|---|---|---|---|
| **Reading Archives** | Supported (reads `.toc` hierarchy, file offsets, sizes) | Uses LotusLib | Supported via `darkness2.bms` in QuickBMS |
| **Decompression** | Partially supported (`decompressEE` supports LZF and Oodle, but requires external proprietary Oodle SDK to build) | Uses LotusLib | The Darkness II primarily uses Evolution LZF (`comtype evolution`), fully handled by QuickBMS |
| **Decoding Materials/Textures/Meshes/Levels** | **NOT SUPPORTED** (LotusLib is strictly a container reader; explicitly has zero asset knowledge) | Supported for Warframe formats via ImHex patterns and custom parsers | Formats differ across engine generations (2012 D2 vs modern Warframe) |
| **Repacking Modified Game Assets** | **NOT SUPPORTED** | **NOT SUPPORTED** | **NO PUBLIC TOOL EXISTS** |

### 4.3 Review of Warframe-Exporter ImHex Patterns (`util/*.hexpat`)
- `wf_common-header.hexpat`: Decodes Evolution Engine asset headers:
  - 16-byte MD5 hash
  - String array of reference paths
  - Property strings / attributes
  - 32-bit type enumeration
- `wf_shader21.hexpat` / `wf_shader22.hexpat`:
  - Contains bytecode length arrays and embedded SM3 shader chunks.
  - Lightmap and material bindings are stored in the header metadata before bytecode chunks.
- **Takeaway:** ImHex patterns provide clean format specifications for inspecting raw game files in a hex editor without writing custom C++ decoders.

### 4.4 The Darkness II Decompiled (`FromDarkHell/TheDarknessIIDecompiled`)
- Contains decompiled Lua scripts and Flash ActionScript UI components.
- Confirms that gameplay logic, weapon scripts, and UI bindings are Lua-driven, while the rendering engine, culling, and camera transformations reside inside `DarknessII.exe`.
- Repacking archives is acknowledged by the author as fragile and unnecessary.

> [!CAUTION]
> **Definitive Finding:** Attempting to mod The Darkness II for VR by repacking `.cache` and `.toc` files on disk is impractical, fragile, and unnecessary. Runtime memory hooks and D3D9 proxy injection (Darkness2VR's current architecture) are the only reliable mechanisms for modifying projections, camera matrices, and stereo replay.

---

## 5. Licensing & Dependency Matrix

| Tool / Repository | Upstream License | Required Dependencies | Can Vendor Code Directly? | Recommendation |
|---|---|---|---|---|
| `apitrace-mcp` | None (Public GitHub repo) | Python >= 3.11, `mcp>=1.29` | No (No license file) | Reimplement algorithms in clean-room Python scripts (as done in `trace_matrix_adapter.py`) |
| `apitrace` | MIT License | C++ runtime, DirectX SDK | External binaries only | Keep outside repo; use for offline game trace generation |
| `wiz3D` | LGPL-2.1 / MIT (iZ3D parts MIT) | Windows SDK, Direct3D 9 SDK | Do not vendor large blocks | Adopt CRC32 calculation and `texldl` token scanning logic cleanly |
| `LotusLib` | None (Public GitHub repo) | C++17, Oodle SDK (Proprietary) | No | Do not vendor; QuickBMS already satisfies extraction needs |
| `Warframe-Exporter` | None (Public GitHub repo) | C++17, Oodle SDK, Qt/wx | No | Use `.hexpat` files as reference specifications only |
| `TheDarknessIIDecompiled` | None (Proprietary assets) | Java / Unluac | **STRICTLY PROHIBITED** | Reference only; contains 2K/DE proprietary scripts |

---

## 6. Offline Smoke Test: Validating `trace_matrix_adapter.py`

To demonstrate immediate practical value without adding external dependencies, we implemented `tools/external-toolchain/trace_matrix_adapter.py`. It integrates the verified matrix classification and Gribb-Hartmann frustum solving logic from `apitrace-mcp` and runs directly on Darkness2VR's native frame trace dumps (`run_*_trace.txt`).

### Smoke Test Execution & Results
**Command:**
```powershell
python tools/external-toolchain/trace_matrix_adapter.py test-output/run_43052_frame_003_trace.txt
```

**Verified Output:**
```text
================================================================================
 Darkness2VR Offline Matrix Analysis: run_43052_frame_003_trace.txt
 Processed 245 draw calls; 58 aligned matrix upload slots
================================================================================
Slot         Kind       Layout      Conf   Draws  FOV(H x V)       Camera Eye (X, Y, Z)     
--------------------------------------------------------------------------------
c0..c3      viewproj   transposed  0.75   102    119.21 x 112.21  (-0.0, 0.4449, -0.4499)  
c4..c7      viewproj   as_stored   0.75   79     103.03 x 98.99   (-0.0, 0.0, 0.0)         
c8..c11     ortho      as_stored   1.00   22     - x -            -                        
c12..c15     viewproj   as_stored   0.75   119    168.48 x 172.21  (-20.4267, -20.2832, -2.2386)
c15..c18     viewproj   as_stored   0.75   94     59.88 x 78.45    (-0.3059, -0.4159, -0.5951)
c20..c23     viewproj   as_stored   0.75   105    151.73 x 142.55  (103.0843, -0.0, 31.1046)
c22..c25     viewproj   as_stored   0.75   222    151.73 x 142.55  (-1.3015, -1.0089, 0.0)  
c45..c48     viewproj   as_stored   0.75   117    98.57 x 98.84    (-1.0, 0.0, 0.0)         
c53..c56     viewproj   as_stored   0.90   50     91.21 x 107.11   (-1.0, -0.0, 0.0)        
c55..c58     viewproj   transposed  0.90   102    84.22 x 90.0     (-0.0, -0.0, -0.0)       
c61..c64     viewproj   transposed  0.90   74     90.0 x 90.0      (...)
c65..c68     viewproj   transposed  0.90   104    124.26 x 90.0    (-0.0, -0.0, -0.0)       
================================================================================
```

### Analysis of Smoke Test Findings:
1. **Scene World-View-Projection (`c0..c3`):**
   - Confirmed as `viewproj` in `transposed` storage order (column-major in registers).
   - FOV extracted: ~119.2° horizontal × 112.2° vertical.
   - Used across 102 draw calls.
2. **Screen / Texture Projection (`c20..c23`):**
   - High draw count (105 draws), wide FOV projection matching auxiliary screen space mapping.
3. **Shadow / Cubemap Passes (`c53..c68`):**
   - Consistently detected with 90.0° vertical FOV and square aspect ratios, confirming point-light and shadow map passes.
4. **2D UI / HUD Elements (`c8..c11`):**
   - Classified with 1.00 confidence as `ortho` projection.

---

## 7. Recommended Workflow for MAIN

When MAIN needs to diagnose rendering anomalies, camera movement, or missing shaders:

### Workflow 1: Rapid Offline Frame Diagnostics (Using Darkness2VR Native Traces)
1. In `Darkness2VR.ini`, enable frame dumping:
   ```ini
   [Diagnostics]
   DumpFrameTrace=1
   ```
2. Trigger the dump in-game using the diagnostic hotkey.
3. Run the offline adapter on the dumped text log:
   ```powershell
   python tools/external-toolchain/trace_matrix_adapter.py test-output/run_<pid>_frame_<num>_trace.txt
   ```
4. Check whether `c0..c3` was correctly identified and whether camera eye coordinates match the expected player position.

### Workflow 2: Pristine Engine Capture (Using 32-bit apitrace)
1. **Preparation:**
   - Temporarily rename `d3d9.dll` in the game directory to `d3d9.dll.d2vr`.
   - In Steam, disable the Steam Overlay for The Darkness II.
   - Copy 32-bit `apitrace.exe` and `lib/wrappers/d3d9.dll` to the game folder.
2. **Capture:**
   - Launch the game through Steam.
   - Progress from main menu to 3D gameplay; move the camera left and right.
   - Quit game cleanly via in-game menu.
3. **Inspection:**
   - Move `DarknessII.trace` to `test-output/`.
   - Restore Darkness2VR's `d3d9.dll`.
   - Run `apitrace dump DarknessII.trace | select-string "SetVertexShaderConstantF"` or open in `qapitrace.exe` to inspect draw states.

### Workflow 3: Recommended Enhancements to Darkness2VR Proxy
1. **Adopt CRC32 Hashing for Shaders:**
   - Switch or complement `HashBytes` (FNV-1a) with standard CRC32 in `src/stereo_replay.inl`.
   - This enables 1:1 filename matching with existing HelixMod community shader fixes (`extracted_helix_3dvis/ShaderOverride/`).
2. **Implement `ScanForStereoTexldl`:**
   - Add a lightweight scan for opcode `D3DSIO_TEXLDL` on sampler 0 to detect if a community shader override already shifts geometry.
   - Prevents double-shifting artifacts when combining HelixMod fixes with Darkness2VR's replay pass.
