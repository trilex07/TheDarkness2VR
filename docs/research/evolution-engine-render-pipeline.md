# Evolution Engine render pipeline (The Darkness II) — research log

Branch: `research/evolution-engine-render-pipeline`  
Status: **in progress — ROOT CAUSE NOT YET PROVEN**

This document is the single source of truth for frame lifecycle and renderer archaeology. Measured data overrides prior agent commentary.

---

## Engine identification

| Item | Evidence |
|------|----------|
| Title | The Darkness II (Steam 67370), `DarknessII.exe` PE32 x86, **ASLR off** (fixed VAs) |
| Engine | Digital Extremes **Evolution Engine** (deferred + cel ink), same broad lineage as *Dark Sector*; not 1:1 with modern Warframe |
| Graphics API | Direct3D 9Ex (`Direct3DCreate9Ex` path documented in `ENGINE_REVERSE_ENGINEERING.md`, `RE_FINDINGS.md`) |
| Deferred | INTZ depth, `SS_InverseProjection` / `SS_InverseViewProjection` on PS c27–c30 (Helix shadow fix uses reg 27) |

---

## What we know about D3D9 hook observations (mod logs)

| Event | Observed in TD2 + our proxy |
|-------|-----------------------------|
| `BeginScene` / `EndScene` | **Yes**, high rate (~88–90/s logged), often **multiple nested** pairs per perceived frame |
| `IDirect3DDevice9::Present` @ vtable 17 | **Not logged** in user sessions (`Present@17` never seen) |
| `PresentEx` @ 121 | Unknown / not seen in cited logs |
| `IDirect3DSwapChain9::Present` @ vtable 3 | **Hook existed in code but was not installed** until research branch; **must be measured** with `RenderPipelineTrace=1` |

**Open issue:** Absence of `Present@17` in our log does **not** prove the game never presents — only that this device vtable path was not hit (wrapper, swapchain-only present, or different device instance).

---

## Frame boundary policies (code)

Implemented in `src/frame_boundary.h` and wired from `src/d3d9.cpp` via `[Render] FrameBoundaryPolicy`:

| Value | Name | Behavior |
|-------|------|----------|
| 0 | `PresentPrimary` | `VrFrame` only from `DevicePresent` / `PresentEx` / `SwapChainPresent` |
| 1 | `LegacyRc20b` | If no present seen: outermost `EndScene` + **9 ms debounce** → `VrFrame` |
| 2 | `TraceOnly` (research default) | **No** `VrFrame` from boundary machine — capture traces without XR flood |

### rc20b debounce — corrected math

- Interval: **9 ms** (`kLegacyEndSceneDebounceMs`)
- Maximum sustained rate if every outer `EndScene` qualifies: **1000/9 ≈ 111.1 Hz**, **not** ~1 Hz
- Prior natural-language report (“~1 VrFrame/s”) was **wrong**
- At ~90 EndScene/s with debounce, expect **~80–90 full `VrFrame` paths/s** when `rec >= 500` still runs replay — that matches loading stalls

### `rec < 500` skip

- Still in `VrFrame()` for replay only; **not** a frame boundary definition
- Distribution by game phase (menu / loading / gameplay) **not yet measured** — OPEN

---

## Instrumentation

Enable in `%LOCALAPPDATA%\Darkness2VR\Darkness2VR.ini`:

```ini
[Render]
RenderPipelineTrace=1
RenderPipelineTraceMax=200000
FrameBoundaryPolicy=2
```

Output: `%LOCALAPPDATA%\Darkness2VR\render_pipeline_trace.csv`  
Columns: `ms,thread,event,sceneDepth,draws,records,device,note`

Sequences to capture (manual/automated): main menu, loading, gameplay entry, gameplay, pause, cutscene.

---

## Offline validation

| Suite | Path | Scale (current) |
|-------|------|-----------------|
| Frame boundary FSM + rc20 regression | `tools/frame-boundary-simulator/` | 50k synthetic traces + 25k timing scenarios |
| Stereo math | `tools/stereo-math-tests/` | ~212k assertions (existing) |
| Full gate script | `tools/run-full-vr-validation.ps1` | Aggregates build + suites |

Permanent regression: `test_rc20_infinite_loading_regression` in `frame_boundary_suite.cpp`.

---

## Helix / NVIDIA 3D Vision fix (same game)

Package in repo: `darkness_ii.3dvis.zip` → `extracted_helix_3dvis/`

| Finding | Detail |
|---------|--------|
| Source | [Helix Mod — Darkness II update (2012-04-04)](https://helixmod.blogspot.com/2012/04/darkness-ii-3d-vision-fix-update.html), mirror `https://s3.amazonaws.com/-HeliX-/darkness_ii.3dvis.zip` |
| Install | `d3d9.dll` + `DX9Settings.ini` + `ShaderOverride/` next to `DarknessII.exe` |
| **`UseEndScene=false`** | Helix did **not** treat EndScene as the stereo frame owner |
| Shadow fix | `[PSC891DB74]` — matrix from PS reg **27**, inverse, write reg 100 (stereo unprojection class bug) |
| VS overrides | 14 hashes with `WorldViewProjection` in overridden HLSL |
| Known limits | Weapon/Demon Arms split at high convergence (author: no fix); cutscene/HUD issues in forum reports |

Detailed shader table: `docs/research/external-evolution-stereo-research.md`.

---

## Renderer passes (hypothesis → to prove)

From `ENGINE_REVERSE_ENGINEERING.md` + Helix:

- World geometry: `SS_WorldViewProjection` c0–c3
- Cel / screen-space: `SS_ScreenTextureProjection`
- Deferred lighting: `SS_InverseProjection`, `SS_InverseViewProjection`
- Shadows: separate PS path (Helix PSC891DB74)
- First-person / Demon Arms: **assume separate projection** until matrix clustering proves otherwise

---

## Why rc20 was wrong

1. **Wrong boundary:** `EndScene` outer close is not proven 1:1 with displayed frame when Present is on another path.
2. **XR ownership:** `VrFrame` → `BeginTrackedFrame` / `SubmitXR` from debounced EndScene injects **`xrWaitFrame` on render thread** at ~90–111 Hz.
3. **Loading:** Low `rec` count still paid XR wait when debounce allowed; `rec < 500` reduced GPU replay but **not** wait frequency.

---

## Target architecture (not shipped)

1. Prove presentation path via trace (SwapChain vs Device vs Ex).
2. One logical game frame → one `VrFrame` (PresentPrimary policy).
3. Classify replay records by matrix/shader cluster before stereo WVP edits.
4. Separate stereo math validation from frame boundary (already split in tests).

---

## REAL GAME TRACE (2026-10-21 automated runs)

**Install discovery:** Steam AppID **67370**, `Find-Darkness2Game.ps1` →  
`C:\Program Files (x86)\Steam\steamapps\common\Darkness II\DarknessII.exe` (manifest `appmanifest_67370.vdf`).

**Deployment:** `tools/deploy-research-build.ps1 -Install` (SHA256 verify + `%LOCALAPPDATA%\Darkness2VR\backups\`).

### Runtime evidence (d3d9.log, research DLL)

| Observation | Evidence |
|-------------|----------|
| **SwapChain::Present@3 fires** | Log: `SwapChain::Present: complete game-frame boundary` after device reset |
| **Device::Present@17 hook not hit** | No `Hook_Present@17 detected` line; vtable slot `[17]=580DEFE0` (system `d3d9.dll`) |
| **Hooked device** | e.g. `device 0F7C4EC0 vtable=0F7C7E1C` |
| **Engine device global mismatch** | `[FovRE] engine device global @0x010DEE90 = 0x000000C9` vs hooked device pointer (separate COM object / wrapper path) |
| **EndScene hooked** | vtable `[42]=580DEE20` |

**Working hypothesis (high confidence):** displayed frames are finalized via **`IDirect3DSwapChain9::Present`**, not `IDirect3DDevice9::Present`. This matches Helix `UseEndScene=false`.

**CSV trace:** blocked on ini key placement + short-lived process during automation; `LoadConfig` now accepts `RenderPipelineTrace` outside `[Render]`. Re-run `tools/run-darkness2-trace.ps1` for quantitative rates.

### Static EXE scan (`tools/analyze_darkness2_present.py`)

Direct `FF 90 disp32` vtable call patterns for Present/EndScene: **0 hits** in `.text` (likely indirect calls through stored device/swapchain vtables).

---

## NEXT BLOCKING TASKS

- [ ] Capture and publish trace CSV summaries for menu / loading / gameplay
- [ ] Statically locate `Present` / `SwapChain::Present` call sites in `DarknessII.exe`
- [ ] Record-count histograms per phase
- [ ] D3D9 harness + image disparity tooling
- [ ] Hang watchdog + automated menu→gameplay cycles
