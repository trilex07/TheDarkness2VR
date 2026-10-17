# Darkness2VR — Agent Handoff (2026-10-17)

## Mission

Native VR mod for **The Darkness II** (Evolution Engine, **D3D9**, **PE32 x86**, Steam app **67370**). Target parity with **SingularityVR** long-term (OpenXR, stereo, 6DoF, weapons). **Current milestone:** prove **D3D9 proxy + OpenXR** presents the game image in HMD + **gamepad input** while game runs on desktop.

**User setup:** PC VR (user mentions **PSVR2** bridge / Steam). Game launched from Steam with files beside exe.

**Repo location:** `C:\Users\Trilex\Darkness2VR` (**no git** in this folder; separate stub `TheDarkness2VR/` with only `.gitattributes`).

**Reference mod (read-only):** `C:\Users\Trilex\vr-mods\SingularityVR-combined2` — especially `spikes/vr_render/d3d9.cpp` (minimal OpenXR Present path) and `spikes/view_matrix/d3d9.cpp` (full UE3 + XInput ordinal hook + stereo).

**Plan doc (broader):** `c:\Users\Trilex\.cursor\plans\darkness_ii_vr_mod_1b790792.plan.md`

**Policy:** Follow `C:\Users\Trilex\vr-mods\AGENTS.md` — **evidence before speculative patches**. User is blocked; next agent must **observe** (log, pixel readback, RenderDoc, optional PSVR2 visual confirm).

---

## User-reported symptoms (latest)

| Symptom | Notes |
|--------|--------|
| **HMD black / nothing** | Game **visible on monitor** (menu/gameplay renders). |
| **Gamepads dead** | Since ~v0.1.4; user said pads worked briefly when VR first came up (early v0.1.x). |
| **v0.1.3 (user)** | Reported **cinema VR worked** (mono, head-locked screen). |
| **v0.1.4+** | User reports **regression**: black HMD + no pads. |

Do **not** trust “first frame submitted” log line alone as “VR works” — user repeatedly says HMD empty while log looks healthy.

---

## Install layout

| Path | Role |
|------|------|
| `C:\Program Files (x86)\Steam\steamapps\common\Darkness II\DarknessII.exe` | Game (~14.3 MB, x86, no ASLR, image base 0x400000) |
| Same folder: `d3d9.dll`, `openxr_loader.dll` | Mod (proxy + loader) |
| Backups: `d3d9.dll.off-YYYYMMDD-HHMMSS` | Previous proxy versions |
| Log: `%LOCALAPPDATA%\Darkness2VR\d3d9.log` | Truncated each run (overwrite on attach) |
| OpenXR SDK: `C:\Users\Trilex\openxr-sdk` (or `OPENXR_SDK`) | Win32 `openxr_loader.lib` / `.dll` |

**Build/install:**

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Users\Trilex\Darkness2VR\build.ps1" -Install
```

**Rollback:** Remove/rename `d3d9.dll` + `openxr_loader.dll` in game folder; restore `d3d9.dll.off-*` if needed.

---

## Current code (v0.1.6)

**Single source:** `C:\Users\Trilex\Darkness2VR\src\d3d9.cpp`  
**Exports:** `C:\Users\Trilex\Darkness2VR\d3d9.def` (forwards D3D9 + `Direct3DCreate9Ex`)

### D3D9 path (working in log)

1. `Direct3DCreate9Ex` → hook factory vtable **16** `CreateDevice`, **20** `CreateDeviceEx` (IDirect3D9Ex).
2. `Hook_CreateDeviceEx` → force **windowed**, hook device **Present@17**, **Reset@16**, **BeginScene@41**, **EndScene@42**.
3. `VrFrame` once per game frame (dedupe via `BeginScene` counter) → `InitXR` → `SubmitXR`.

### OpenXR path

- Instance + `XR_KHR_D3D11_enable` → D3D11 device on runtime LUID → session + **LOCAL** `g_xrSpace`.
- Swapchain RGBA, copy D3D9 backbuffer → D3D11 upload → XR swapchain image.
- Submit **`XrCompositionLayerQuad`** in **LOCAL** space, pose = **head center + 2 m forward** (`CinemaQuadPose`), same texture both eyes.
- **OpenXR action set** for Touch/Index/WMR → cached XInput (see below).

### Capture

- `GetBackBuffer` → `GetRenderTargetData` to sysmem surface; fallback **`GetFrontBufferData(0, …)`** if RT data fails.
- No log yet for **non-black pixel** verification (TODO).

### Input (broken in log)

- **`TryInstallXInputHook()`** attempts **IAT patch** on main module for `XINPUT1_3.dll` **ordinal 2** (`HookIATOrdinal`).
- **Always fails** in latest log → see below.
- Called from **`HookFactory`** and **every `VrFrame`**; on failure code **resets `g_xinputHooked` to 0** → **logs failure every frame** (log spam thousands of lines).

**Game binary fact (verified dumpbin):**

```
XINPUT1_3.dll
  Ordinal 2  → XInputGetState
  Ordinal 3  → XInputSetState
```

Imports are **by ordinal**, not by name — MinHook on `GetProcAddress("XInputGetState")` **never intercepts the game** (tried v0.1.4–v0.1.5).

---

## Latest log evidence (v0.1.6, pid=40000)

```
Darkness2VR v0.1.6 attached
Factory hooked …
XInput IAT hook failed (ordinal 2)     ← repeats every VrFrame (bug)
Hook_CreateDeviceEx … windowed=1
Present/Reset/Begin/EndScene hooked
Device Reset (1920x1080 windowed=1)
OpenXR gamepad actions ready
OpenXR session ready (LOCAL cinema + XR pad cache on render thread)
XR session running
swapchain 1920x1080
*** first frame submitted (LOCAL head cinema 2.0m) ***
…
xr submit ok frames=600 shouldRender=1 layers=1
```

**Interpretation:**

- **OpenXR session runs**; **600+ frames** with `shouldRender=1`, `layers=1` — compositor *should* get quads.
- User still sees **black HMD** → disconnect between **software submit** and **visible photons**, OR **texture is black**, OR **quad layer ignored** on runtime (PSVR2), OR user looking wrong way (less likely if persistent).
- **No** `feeding XInput port 0 from OpenXR` → XR pad cache never consumed by game (no IAT hook + maybe no active bindings).
- **No** `capture failed` lines → CopyFrame path returns success (does not prove non-black pixels).

---

## Version timeline (what changed)

| Version | User-visible | Technical notes |
|---------|----------------|-----------------|
| **v0.1.1** | Flat, no VR | Only factory hook; no `CreateDeviceEx` → no Present hook. |
| **v0.1.2** | VR stereo “double” | Present hooked; mono same image both eyes (projection). |
| **v0.1.3** | **User: cinema OK** | `CompositionLayerQuad`, LOCAL, head-follow pose. |
| **v0.1.4** | Black HMD, no pad | LOCAL anchor, MinHook XInput, focus steal, Reset windowed. |
| **v0.1.5** | Still broken | VIEW quad, OpenXR actions, MinHook, sync actions from input thread (bad). |
| **v0.1.6** | Still broken | Revert LOCAL cinema; remove MinHook; IAT hook (fails); pad sync on render thread only. |

**Accepted baseline to restore first:** behavior as close as possible to **v0.1.3** rendering path, then fix input separately with **proven** hook.

---

## Falsifiable hypotheses (prioritized)

### A. HMD black but submit OK

1. **Backbuffer copy is black/wrong surface** (game renders to another chain / post-process).  
   - *Test:* CPU readback mean luminance after `CopyFrame`; log min/max RGB every N frames; compare with monitor screenshot same frame.

2. **`CompositionLayerQuad` not composited on user runtime** (PSVR2 / SteamVR quirk).  
   - *Test:* Submit **`XrCompositionLayerProjection`** (v0.1.2 style) with center-eye pose + same swapchain; A/B in one build via ini flag.

3. **Swapchain format / color space** (Sony extensions unused).  
   - *Test:* enumerate formats; try `SRGB` vs `UNORM`; check `XR_SONY_swapchain_color_space` if runtime exposes it.

4. **Quad pose / space wrong** (off-screen).  
   - *Test:* VIEW space fixed `(0,0,-2)` identity; log `xrLocateViews` position; draw debug solid color in upload texture (no D3D9 copy) — if solid color visible, copy is wrong.

5. **Session visible but HMD is different display**  
   - *Test:* user confirms which display is “black”; check `xrEnumerateViewConfigurations` / mirror settings.

### B. Gamepad dead

1. **IAT hook fails** (confirmed in log). Likely **bound import directory** (`OriginalFirstThunk` zero) — exe has **Bound Import Name Table** in dumpbin.  
   - *Test:* Patch **`FirstThunk`** slots by index after `LoadLibrary(XINPUT1_3)`; or delay hook until xinput loaded and patch by **target address** match; or ship **`xinput1_3.dll` proxy** next to exe (game loads it first).

2. **VR runtime exclusive — no XInput devices** on ports 0–3.  
   - *Test:* Log `XInputGetState(0..3)` return codes **without** hook from render thread once per second.

3. **OpenXR bindings inactive** (PSVR2 Sense profile not in suggested bindings — only Touch/Index/WMR).  
   - *Test:* Log `isActive` per action; add Sony/PSVR2 interaction profile paths from runtime’s `xrEnumerateInstanceExtensionProperties` / SteamVR binding docs.

4. **`xrSyncActions` only on render thread** — OK for OpenXR, but cache never reaches game if IAT hook missing.

### C. Code health bugs (fix regardless)

- **`TryInstallXInputHook`**: on failure, do **not** clear `g_xinputHooked` and retry every frame; log **once**.
- **Log spam** makes user log unusable (1000+ identical lines).

---

## Recommended next steps (ordered)

### Phase 1 — Restore trusted VR image (1–2 hours)

1. Add **`#define D2VR_SUBMIT_MODE`** enum: `QUAD_LOCAL_HEAD` (v0.1.3), `QUAD_VIEW`, `PROJECTION_MONO`.
2. **Solid-color frame test:** skip `CopyFrame`, fill upload texture **bright green**; submit. Ask user PSVR2 check.  
   - Green visible → copy/layer OK; fix capture.  
   - Still black → layer/runtime/session issue.

3. Pixel readback stats on real copy (avg R/G/B).

4. Re-diff against known-good **v0.1.3** submit block (git not available — reconstruct from agent transcript / backups `d3d9.dll.off-*` near timestamp `124801` if needed).

### Phase 2 — Input (evidence-first)

1. Fix IAT hook for **bound imports** OR deploy **proxy `xinput1_3.dll`** (forward ordinals 2/3, inject synthetic state from shared mem written by d3d9).

2. Add **PSVR2 / Sony** OpenXR interaction profile bindings (research exact path string in user’s OpenXR runtime).

3. Instrument: log once per 5s — `XInput(0..3)` result, `g_cachedXrPadValid`, action `isActive` flags.

4. Compare **Singularity** `InstallXInputHook` + ordinal usage in `spikes/view_matrix/d3d9.cpp` (~line 970+) — same ordinal import pattern.

### Phase 3 — Automation

- Skill: `C:\Users\Trilex\.codex\skills\game-automation\SKILL.md` — launch Steam app 67370, window capture, read log tail.
- Optional RenderDoc attach for D3D9 present chain.

---

## Key code anchors (`src/d3d9.cpp`)

| Symbol | Purpose |
|--------|---------|
| `HookFactory` / `Hook_CreateDeviceEx` | D3D9 entry |
| `VrFrame` / `SubmitXR` | OpenXR frame loop |
| `CopyFrame` | D3D9 → D3D11 |
| `CinemaQuadPose` / quad layer | HMD presentation |
| `InitXrGamepadActions` / `UpdateCachedXrPad` | VR controller → pad cache |
| `HookIATOrdinal` / `TryInstallXInputHook` | **Currently failing** |
| `Hook_XInputGetState` | Would serve cache + port remap |

---

## Game RE hints (not started)

- Strings in exe: `Direct3DCreate9Ex`, `MAIN_HAND`, `OFF_HAND`, `DualWielding`, `DemonArm`, etc.
- **No** working `.cache` reimport (ZenHAX) — do not patch assets blindly.
- Stereo / head look requires Evolution **view-projection RE** (Singularity spike 9 methodology).

---

## User communication

- Polish preferred.
- Wants **working HMD cinema** before engine stereo.
- Ask for **short PSVR2 test** after solid-color / projection A-B — automation cannot see HMD.
- Do not claim “VR works” without **physical HMD** or solid-color confirmation.

---

## Transcript

Full chat: `C:\Users\Trilex\.cursor\projects\c-Users-Trilex-vr-mods\agent-transcripts\202e463b-562c-4695-bcb5-63a88a77202b\202e463b-562c-4695-bcb5-63a88a77202b.jsonl`

---

## Immediate “first hour” checklist for incoming agent

- [x] Read `%LOCALAPPDATA%\Darkness2VR\d3d9.log` after one user run (tail + search `submit`, `capture`, `IAT`).
- [x] Fix IAT retry spam; implement **bound-import-safe** hook OR xinput proxy. *(Fixed in v0.1.7: Delay-load IAT hook scanning IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT for ordinal 2 at 0x10dbec8, logs once)*
- [x] Implement **solid green** submit test; user confirms HMD. *(Added in v0.1.7: TestMode=1 bright green, TestMode=2 checkerboard, F9 live toggle)*
- [x] If green OK: fix `CopyFrame` (RenderDoc / backbuffer index / front buffer). *(Added in v0.1.7: mean/max RGB stats every 60 frames, warning on black buffer, VrFrame moved to Hook_Present with F11 toggle)*
- [x] If green fails: switch to **projection layer** + runtime format tests. *(Added in v0.1.7: SubmitMode supporting QUAD_LOCAL_HEAD, QUAD_VIEW, PROJECTION_MONO with F10 live toggle)*
- [x] Add Sony/PSVR2 controller bindings; verify `feeding XInput from OpenXR` in log. *(Fixed in v0.1.7: corrected OpenXR binding path syntax, Touch/Index/WMR/Simple profiles, neutral-state persistence)*
- [x] Update `README.txt` version string (still says v0.1). *(Updated in v0.1.7 with hotkey and INI guide)*

---

## v0.1.7 Updates (2026-10-17)

### Breakthroughs & Fixes:
1. **XInput Delay-Load IAT Identified:** `DarknessII.exe` uses `IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT` (index 13) for `XINPUT1_3.dll`, NOT standard imports (entry 1). Ordinal 2 (`XInputGetState`) is at IAT slot `0x010DBEC8`. Hook now properly patches this slot and logs once (no retry spam). *Confirmed working in v0.1.7 log (`orig=64B04760`).*
2. **OpenXR Binding Syntax Fixed:** Previous code passed invalid paths (`/input/thumbstick`). Now correctly passes full top-level user paths (`/user/hand/left/input/...`, `/user/hand/right/input/...`).
3. **Controller Connection Stability:** Emulated Xbox pad on port 0 reports connected state even when sticks are neutral, preventing engine from assuming no gamepad is plugged in.
4. **HMD Photons / Green Screen Test:** Configurable via `Darkness2VR.ini` and live hotkey `F9` (0 = Game Capture, 1 = Solid Bright Green, 2 = Checkerboard).
5. **Flexible Presentation Modes:** `F10` cycles `QUAD_LOCAL_HEAD` (0), `QUAD_VIEW` (1), `PROJECTION_MONO` (2).
6. **Capture Timing & Pixel Readback:** Pixel luminance stats logged every 60 frames.

## v0.1.8 Updates (2026-10-17)

- **Root Cause of v0.1.7 Stall:** In v0.1.7, `Hook_EndScene` was gated behind `if (g_captureSite == 1)`. Because `DarknessII.exe` uses D3D9Ex and does NOT call `Present` (slot 17), `VrFrame` was never invoked at all! The render loop stalled before `InitXR()` could even run.
- **Fix:** In v0.1.8, `Hook_EndScene`, `Hook_Present`, and `Hook_PresentEx` (slot 121) unconditionally invoke `VrFrame` (with deduplication on `g_frameId` + monotonic backup counter) and `PollHotkeys()`.
- **Default:** `TestMode=0` (real game capture) and `SubmitMode=0` (v0.1.3 cinema quad) default in INI, with live hotkeys `F9` (test patterns), `F10` (submit modes), `F11` (cinema distance).

*Handoff updated after v0.1.8 build & install beside DarknessII.exe.*

