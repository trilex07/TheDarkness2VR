# Handoff: native gaze LOD ownership

**Date**: 2026-11-01
**Task ID**: gaze-lod-optimization-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed gaze-dot/R2 candidate DLL `E6F49A7E...`; accepted rollback remains source `4a26aabd`.
**Integration commit**: `0541741151b5f7dff1a0843abe3a5c9a07974c07`; deployment receipt follows.

## Goal

Identify the actual native LOD owner and resource classes in gameplay before a gaze-based CPU quality change.

## Status

Read-only runtime candidate installed and verified; headset observation pending.

## Context Loaded

- [x] BASELINE.md
- [x] FAILED-APPROACHES.md checked
- [x] `docs/research/gaze-lod-resume.md` and `HANDOFF-gaze-lod-diagnostic.md`
- [x] Stock-EXE Ghidra exports `gaze-lod-selector.txt`, `gaze-lod-call-path-2.txt`, `gaze-lod-view-entry.txt`

## Changes

- `src/gaze_lod_runtime_observer.inl`: opt-in single-sample native dispatcher witness at `008513A0`. Uses the existing hardware execution breakpoint implementation; no EXE, IAT, vtable, object or LOD write. Samples only while 3D gameplay is active and disarms on the owning EndScene thread.
- `src/d3d9.cpp`: installs and reports the opt-in witness.
- `tools/deploy-gaze-lod-runtime.ps1`: full nine-target rollback, exact DLL/EXE/profile checks and one INI setting insertion.

## Evidence

- Stock EXE SHA256 `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`; file bytes at RVA `4513A0`: `83 EC 08 80 3D 54 DE 10 01 00`.
- `008513A0`: thiscall dispatcher, `ECX` renderer and `[ESP+4]` view list. `00851462` calls per-object selector `00AE30C0`; alternate `006BF3D0` remains possible.
- Prior camera `+2A4` owner was null in live gameplay; do not reuse that route.
- Protected x86 candidate DLL SHA256 `2AB582630B09EF02AD44465582A74B7FCEB563EB1EA5B4D998C03D8FB716A327`; profile preflight PASS.
- Existing hardware execution boundary fixture PASS (10,000 callbacks, register/flags preservation, 20 rearm cycles, unchanged EXE text). Synthetic 15.432 us/hit is not game timing.
- Installed at `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/gaze-lod-runtime-20261101T110811047`; installed DLL, stock EXE, nine rollback backup hashes and active observer setting verified. Game closed and not launched by agent.

## Next Steps

1. User runs the installed candidate in a gameplay scene; inspect `[GazeLOD]` selector call/sample/stopped records.
2. Resolve observed object/resource class vtables and selected mesh consumer. Measure selector and submit time on a normal build without the hardware observer.
3. Only then trial a bounded, decoration-only gaze LOD adjustment; compare same-scene app CPU time and both eyes.

## Blockers

Native object classification and mesh cost remain unproven. This candidate has no CPU saving.
