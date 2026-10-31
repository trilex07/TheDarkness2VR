# Handoff: High-Res Texture Extraction & Preparation (2K / 4K / 8K)

**Date**: 2026-10-31
**Agent**: opencode (SIDE)
**Branch**: side/texture-hires-extraction-2k4k8k
**Worktree**: current (C:\Users\Trilex\Darkness2VR)
**Baseline**: origin/main (1051c5c6)

## Goal
Extract weapon/tentacle/enemy textures from The Darkness II game packages, prepare source folders + automation script + AI upscaling instructions so MAIN can produce 2K/4K/8K textures and install them in the game dir.

## Status
Done (SIDE offline work complete; MAIN executes extraction/upscaling/install on Steam)

## Context Loaded
- [x] BASELINE.md
- [x] FAILED-APPROACHES.md (checked)
- [x] tools/darkness2.bms (QuickBMS script for Evolution Engine toc/cache)
- [x] quickbms.zip present in repo root
- [x] Game dir located at C:\Program Files (x86)\Steam\steamapps\common\Darkness II (Cache.Windows\B.Texture/H.Texture/F.Texture found)

## Changes
- Files added:
  - tools/prepare_hires_textures.ps1 — automation script: Extract / Sort / Prepare modes
  - docs/research/texture-hires-pipeline.md — full MAIN instructions (install AI, upscale 2K/4K/8K, DDS conversion, deploy)
  - docs/research/texture-hires-extraction-2k4k8k.md — superseded by texture-hires-pipeline.md (kept for reference)
  - docs/agent-workflow/handoffs/HANDOFF-*.md — this file
  - extracted_textures/source/weapons/ — 62 source PNG textures (guns, pistols, rifles, shotguns, ammo, spikes)
  - extracted_textures/source/tentacles/ — 36 source PNG textures (tentacles, demon arms, stingers)
  - extracted_textures/source/enemies/ — 109 source PNG textures (darklings, demons, bosses, winged, gargoyles)
- Commits: pending

## Evidence
- `tools/darkness2.bms`: QuickBMS 0.12.0 script, opens `toc` + `cache` (FDDE), decompresses `evolution`/`oodle` chunks
- Extraction performed on `Cache.Windows\B.Texture.toc/.cache` (~246 MB) — successful, exit code 0
- 124 weapon/tentacle/enemy textures identified by filename keywords and copied to `extracted_textures/source/`
- `d3d9.dll` proxy does NOT currently have loose-texture override (verified: no texture-replacement code in src/d3d9.cpp)

## Next Steps (MAIN)
1. Run: `.\tools\prepare_hires_textures.ps1 -Mode Extract -GameDir "C:\...\Darkness II"` (extracts B/H/F.Texture)
2. Run: `.\tools\prepare_hires_textures.ps1 -Mode Sort -SourceDir <output>` (sorts to weapons/tentacles/enemies)
3. Run: `.\tools\prepare_hires_textures.ps1 -Mode Prepare -DestDir ".\extracted_textures"` (creates 2k/4k/8k structure + manifest CSV)
4. Install AI upscaler (Real-ESRGAN / waifu2x / Topaz Gigapixel / Upscayl)
5. Upscale `2k/source/*.png` → write to `2k/`, `4k/`, `8k/` per category
6. Convert to DDS with mipmaps (DXT1/DXT5/BC7), power-of-two sizes
7. Install to game dir (MAIN only, Steam exclusive), backup originals first
8. In-game test: weapons, tentacles, enemies at 2K/4K/8K, no crashes, VR stable

## Blockers
- No repacker for toc/cache in repo (extractor only) — may require proxy hook or manual pakiet edit
- SIDE cannot access game files or install AI (Steam, MAIN only) — upscaling is MAIN's responsibility

## Notes
- Focus: bronie (weapons), macki (tentacles), przeciwnicy (enemies)
- Deliver to MAIN: ready-to-use script + folder structure + manifest CSV
- Source textures are PNG with _d/_n/_s/_e/_o/_mask suffixes; normal maps (_n) need care during AI upscaling
