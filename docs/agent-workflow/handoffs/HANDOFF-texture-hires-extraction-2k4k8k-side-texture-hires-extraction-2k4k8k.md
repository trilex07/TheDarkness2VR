# Handoff: High-Res Texture Extraction & Preparation (2K / 4K / 8K)

**Date**: 2026-10-31
**Agent**: opencode (SIDE)
**Branch**: side/texture-hires-extraction-2k4k8k
**Worktree**: current (C:\Users\Trilex\Darkness2VR)
**Baseline**: origin/main (1051c5c6)

## Goal
Prepare offline plan + tooling for extracting weapon/tentacle/enemy textures from The Darkness II pakiet (toc/cache via QuickBMS `tools/darkness2.bms`) and upscaling them to 2K/4K/8K so MAIN can install them in the game dir.

## Status
Done (offline plan + handoff; MAIN performs extraction/upscaling/install on Steam)

## Context Loaded
- [x] BASELINE.md
- [x] FAILED-APPROACHES.md (checked)
- [x] tools/darkness2.bms (QuickBMS script for Evolution Engine toc/cache)
- [x] extracted_assets/ (already extracted scripts, not textures)
- [x] quickbms.zip present in repo root

## Changes
- Files added:
  - docs/research/texture-hires-extraction-2k4k8k.md — full extraction/upscaling/install plan
  - docs/agent-workflow/handoffs/HANDOFF-texture-hires-extraction-2k4k8k-side-texture-hires-extraction-2k4k8k.md — this file
- Commits: pending

## Evidence
- `tools/darkness2.bms`: QuickBMS 0.8.0 script, opens `toc` + `cache` (FDDE), decompresses `evolution`/`oodle` chunks, writes files with original names/paths
- No runtime test performed (SIDE constraint: no Steam launch, no game-dir modification)
- `d3d9.dll` proxy does NOT currently have loose-texture override (verified: no texture-replacement code in src/d3d9.cpp)

## Next Steps (MAIN)
1. Run quickbms with tools/darkness2.bms on game toc/cache to extract assets
2. Identify weapon / tentacle / enemy textures among extracted files (by name + DDS preview)
3. Upscale selected textures to 2K/4K/8K (Real-ESRGAN / waifu2x / Lanczos)
4. Convert to DDS with mipmaps (BC7/DXT5/DXT1), power-of-two sizes
5. Verify whether d3d9.dll supports loose texture override; if not, plan pakiet repack or proxy hook
6. Install to game dir (MAIN only, Steam exclusive), backup originals first
7. In-game test: weapons, tentacles, enemies render correctly at 2K/4K/8K, no crashes

## Blockers
- No repacker for toc/cache in repo (only extractor) — may require proxy hook or manual pakiet edit
- SIDE cannot access game files (Steam, MAIN only) — extraction not yet performed

## Notes
- Focus: bronie, macki (tentacles), przeciwnicy (enemies)
- Deliver to MAIN: ready-to-use instructions + folder structure hires_textures/{2k,4k,8k}/{weapons,tentacles,enemies}
