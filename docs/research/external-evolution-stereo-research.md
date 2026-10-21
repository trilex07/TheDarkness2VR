# External research — Evolution Engine & The Darkness II stereo

Compiled for native OpenXR mod; complements `evolution-engine-render-pipeline.md`.

---

## 1. Exact The Darkness II Helix / 3D Vision fix

### Sources

| URL | Notes |
|-----|--------|
| https://helixmod.blogspot.com/2012/04/darkness-ii-3d-vision-fix-update.html | Author update: shadow fix after game patch |
| https://s3.amazonaws.com/-HeliX-/darkness_ii.3dvis.zip | Historical package (also `darkness_ii.3dvis.zip` in repo) |
| https://s3.amazonaws.com/-HeliX-/DLLS/Release.zip | Helix wrapper DLL (forum troubleshooting) |
| https://helixmod.blogspot.com/2014/01/learning-how-to-fix-games.html | NVIDIA stereo unprojection / shadow theory |
| http://developer.download.nvidia.com/whitepapers/2011/StereoUnproject.pdf | Stereo shadow correction background |

### Recovered layout (`extracted_helix_3dvis/`)

- `d3d9.dll` — Helix D3D9 wrapper (not analyzed in this pass; loads overrides)
- `DX9Settings.ini` — shader routing
- `ShaderOverride/VertexShaders/*.txt` — HLSL VS replacements (D3D9 bytecode text)
- `ShaderOverride/PixelShaders/*.txt` — HLSL PS replacements

### DX9Settings.ini (key lines)

```ini
UseEndScene=false
DefPSViewSizeConst=210
DefVSViewSizeConst=26
[PSC891DB74]
UseMatrix=true
GetMatrixFromReg=27
MatrixReg=100
InverseMatrix=true
```

**Implication for our mod:** Historical working stereo for this title did **not** sync to EndScene; presentation / driver stereo sampler drove separation. Our EndScene fallback is **unproven** relative to this reference.

### Shader override table (hash → role)

| Shader hash | Stage | WVP / matrices | Inverse / shadow | Notes |
|-------------|-------|----------------|------------------|--------|
| C891DB74 | PS | — | Reg 27 → inv → reg 100 | Shadow stereo unprojection (post game-update fix) |
| 3FF3A68C | PS | — | Inverse refs | Deferred / lighting class (parse CTAB in file) |
| B6D033A7 | PS | — | — | Override present |
| C0ABBE1C | VS | WVP + ProjectorTextureProjection0 | — | Projector / cookie style pass |
| 35C42545 | VS | WVP | — | World mesh |
| 4E5148AF | VS | WVP | — | World mesh |
| 5B30D4B6 | VS | WVP | — | World mesh |
| 619E2187 | VS | WVP | — | World mesh |
| 7D737519 | VS | WVP | — | World mesh |
| 7E2F4D28 | VS | WVP | — | World mesh |
| 8578BB35 | VS | WVP | — | World mesh |
| A98DD0DF | VS | WVP | — | World mesh |
| B69924C8 | VS | WVP | — | World mesh |
| B79A7FD2 | VS | WVP | — | World mesh |
| BE670DFD | VS | — | — | No WVP in header (likely screen/UI or special) |
| C9E4D2F3 | VS | WVP | — | World mesh |
| F2CF038A | VS | WVP | — | World mesh |

**Potential equivalence in our mod:** Matrix replay touches `SetVertexShaderConstantF` on WVP/STP/InverseProjection registers (`ProjectionFix` bits, `stereo_replay.inl`); Helix instead patched **specific shader binaries** and shadow reconstruction — we must map hashes to engine shader names via CTAB dump (TODO).

### Weapon / Demon Arms / cutscenes (community)

- High convergence: weapon and Demon Arms **separate** (reported as unfixed in Helix era) → supports **secondary near-camera projection** hypothesis.
- Cutscenes: often broken or flat in 3D fixes of this generation → treat as **alternate camera or video** until traced.

---

## 2. Evolution Engine (general)

| Source | Finding | Relevance |
|--------|---------|-----------|
| `ENGINE_REVERSE_ENGINEERING.md` | Deferred, SS_* register map, fixed VAs | Primary binary RE |
| `RE_FINDINGS.md` | Reset @ 0xB3E045, display manager sizes, GetFOV vtable+0x270 | FOV / reset interaction with VR |
| Digital Extremes lineage | Dark Sector → Darkness II → early Warframe shared DNA | Compare pass structure, not modern Warframe 1:1 |
| Star Trek (2013) / other DE titles | Sparse public D3D9 hook writeups | Low confidence unless shader hashes match |

---

## 3. Dark Sector

| Topic | Status |
|-------|--------|
| Same engine family | Likely similar deferred + D3D9 |
| Public Helix fixes | Search "Dark Sector 3D Vision" for parallel shader-override patterns — **not fully imported in this pass** |
| Relevance | Shader constant layouts and stereo-sensitive PS (shadows) may transfer |

---

## 4. Other D3D9 stereo implementations

| Project | Technique | Reusable? |
|---------|-----------|-----------|
| Helix / 3DMigoto | D3D9 wrapper, shader overrides, stereo sampler | **Yes** — same game proved stereo possible |
| DarkStarSword/3d-fixes | Automated VS texcoord / shadow fixes | Methodology for shader classification |
| OpenXR / Khronos samples | Per-eye projection & swapchain submission | **Yes** for submission math, not engine passes |
| Our `SubmitMode 4` | Record/replay + matrix clip shift + D3D11 composite | Needs pass classification + proven frame boundary |

---

## 5. Reference comparison (OPEN issues)

| Input | Helix / NVIDIA | Our mod (current) | Match? |
|-------|----------------|-------------------|--------|
| Frame owner | Not EndScene (`UseEndScene=false`) | EndScene fallback (legacy) / Present (target) | **NO** until trace proves Present path |
| Separation | Driver stereo + shader UV/w correction | Matrix clip IPD + optional headClip | **Unverified** on headset |
| Shadows | PS reg 27 inverse hack | `ProjectionFix` inverse PS path | Partial overlap — needs hash mapping |

Any unexplained difference remains **OPEN** until traces + harness agree.
