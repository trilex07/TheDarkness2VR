# Startup crash investigation — Darkness2VR (2026-10-21)

## Milestone status

**B. Starts without crashing** (local validation, RC15 candidate) — pending your Steam/headset confirm.

Prior RC14 could crash or fail to start on some paths; root causes below are **evidence-backed**, not guessed.

---

## Architecture (The Darkness II)

| Item | Evidence |
|------|----------|
| Engine | **Evolution Engine** (2K/StarDust lineage), not UE3 |
| Binary | `DarknessII.exe` PE32 **x86**, ImageBase **0x00400000**, **no ASLR** |
| Graphics | **Direct3D 9 Ex** proxy (`d3d9.dll` in game folder) |
| Mod stack | D3D9 hooks → record/replay stereo (`stereo_replay.inl`) → D3D11 blit → **OpenXR** (`XR_KHR_D3D11_enable`) |
| Frame boundary | **EndScene@42** (not SwapChain vtable slot 3 — documented in HANDOFF) |
| RE refs | `RE_FINDINGS.md`, `ENGINE_REVERSE_ENGINEERING.md`, `thunk_disasm.txt` |

```text
DarknessII.exe
  → loads proxy d3d9.dll (DllMain)
  → forwards to system d3d9 + hooks Direct3DCreate9Ex / device vtable
  → game renders to oversized backbuffer (GetClientRect IAT hook)
  → VrFrame: OpenXR + (SubmitMode 4) live pass + replay → swapchains
  → SteamVR / OpenXR compositor
```

---

## Crash hypotheses (tested)

### H1 — Native hands **palette exe patch** (RC7a)

- **Evidence:** `dist/rc7b-startup-fix-20261021/README.md` — intro crash with palette hook; disabled in RC7b+.
- **Current code:** `native_hands.inl` — **no exe patch**; log says “NO additional engine patch”.
- **Conclusion:** Not the current startup crash unless an old DLL is installed.

### H2 — **FOV hooks in DllMain** (RC11–RC14)

- **Evidence:** `InstallGetFovProbe()` ran in `DLL_PROCESS_ATTACH`, patching `DarknessII.exe` during loader lock.
- **Risk:** Intro/menu calls GetFOV paths early; 10-byte frustum patch `@0x00B23F7F` replaces `mov edx,[eax+270]; mov ecx,esi; call edx`.
- **Local run (RC14):** Process alive 45s+; log reaches frame 16k+ — crash is **intermittent / environment-specific** (Steam, HMD, intro timing).
- **Fix (RC15):** Defer FOV patches until **first `HookDevice`**; default **`FovFrustumHook=0`**.

### H3 — **Global F90DAC / FFBA08 writes** (RC11–RC13)

- **Evidence:** AV @ `00F90DAC` (`.rdata`); double-scale projection → exe AV `+0xA129EA` near gameplay.
- **Fix:** Removed global writes in RC14+; use call-site hooks only.

### H4 — **`ShowWindow(SW_HIDE)`** on XR session (RC14)

- **Evidence:** Can confuse Steam focus / mirror window; user reported “2 screens” + instability.
- **Fix (RC15):** Gated behind **`HideGameWindow=1`** (default **0**).

---

## Startup instrumentation (RC15)

Ordered log lines:

```text
[Startup] stage=1  DllMain PROCESS_ATTACH
[Startup] stage=2  VEH crash handler installed
[Startup] stage=3  LoadConfig
[Startup] stage=4  native hooks (or skipped if StartupMode>=1)
[Startup] stage=5  LoadReal system d3d9.dll OK
[Startup] stage=6  TryInstallXInputHook done
[Startup] stage=7  Direct3DCreate9Ex
[Startup] stage=11 HookDevice
[Startup] stage=12 InstallGetFovProbe (post-CreateDevice)
[Startup] stage=14 InitXR session ready
```

On crash, check `%LOCALAPPDATA%\Darkness2VR\d3d9.log` for **last `[Startup] stage=`** and VEH `stage=` / `lastHook=`.

---

## StartupMode (Render.StartupMode)

| Mode | Behavior |
|------|----------|
| **0** | Default: natives + D3D + XR; FOV patches **after CreateDevice**; frustum hook off unless `FovFrustumHook=1` |
| **1** | Skip native aim/weapon/hands/throw hooks (D3D + XR only) |
| **2** | Also skip all FOV exe patches |

Use mode **1** or **2** to bisect: if mode 2 starts and mode 0 crashes → FOV path; if mode 1 starts and 0 crashes → native hook path.

---

## OpenXR validation (without full game)

```powershell
cd test
.\build_vr_test.ps1
```

Runs `vr_frame_test.exe` against real `openxr_loader.dll` (adapter/frame identity tests). **PASS** on 2026-10-21 — OpenXR wrapper logic is not the primary startup failure mode when the game dies before stage 14.

---

## Graphics / FOV RE note

Only **4** `fld dword ptr [0x00F90DAC]` sites in the exe (scan 2026-10-21); global FOV is not the only cull path. Frustum site `0x00B23F7F` uses **call edx** — opt-in only.

---

## VR-framework (elliotttate/vrframework)

**Useful patterns (conceptual):** defer graphics hooks until device exists; separate bootstrap from session; avoid patching game code from DllMain.

**Do not copy blindly:** engine-specific GetFOV / deferred renderer / SubmitMode 4 record-replay are Darkness2VR-specific.

---

## Validation pipeline

```powershell
.\build.ps1
.\test\build_vr_test.ps1
.\tools\validate-startup.ps1
```

---

## Failed experiments (do not retry)

| Experiment | Result |
|------------|--------|
| Write `0x00F90DAC` / `0x00FFBA08` in `.rdata` | AV or silent fail; gameplay crash |
| RC12 double projection scale | Crash ~`0xA129EA` |
| FOV + vtable 0x270 replace | Widens all GetFOV; unstable |
| Palette hands exe patch (RC7a) | Intro crash (documented RC7b) |
| Frustum 10-byte patch from DllMain | High risk at intro; **deferred + default off** in RC15 |

---

## RC15 candidate

- **Branch:** `research/startup-crash-openxr` (when pushed)
- **Build:** `rc15-safe-startup-20261021`
- **Install:** `.\tools\prepare_vr_candidate.ps1 -Install`

**Your test:** Cold start from Steam with headset connected. Success = log passes stage 11–14, no process exit in intro/menu, no VEH line during first 2 minutes.
