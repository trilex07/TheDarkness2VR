# Handoff: single-owner D3D9 performance backlog

**Task ID**: single-owner-d3d9
**Date**: 2026-10-31
**Role**: MAIN
**Branch**: codex/main-single-owner-d3d9-validation
**Status**: Integration and paired test complete; candidate provisional
**Integration commit**: f24ced92f01823f070e1a2b9361d72b920ed6b92
**Runtime evidence commit**: eab97b35
**Worktree**: C:/Users/Trilex/.codex/worktrees/main-render-cpu-next/Darkness2VR
**Baseline**: accepted rollback 4a26aabd; current candidate f24ced92

## Completed
25c63092 mode 4 integrated, both x86 builds and 19 suites PASS. One authorized
mode-1/mode-4 gameplay pair complete, zero PresentMon loss. Same DLL and startup
caches; actual flags 0x54 -> 0x50. Mean app frame 14.73 -> 13.73 ms, p95
18.09 -> 15.50, p99 19.49 -> 16.96 ms. User reports visuals appear OK.
Mode 4 retained provisionally; no stable 90 FPS or objective HMD latency proof.

## Evidence
- ../BASELINE.md
- ../../main-single-owner-d3d9-validation.md
- ../../validation/single-owner-d3d9-20261031.json
- Local raw captures: %LOCALAPPDATA%/Darkness2VR/perf/prepared-single-owner-20261031
- Rollback: %LOCALAPPDATA%/Darkness2VR/packages/single-owner-d3d9-20261031/rollback

## Next performance work
Do not reintegrate 25c63092 or repeat the completed comparison. Review remaining
producer-fence outlier (58.98 ms frame; enclosing Issue/GetData(FLUSH) window
42.656 ms), bounded against existing evidence. Driver causality unproven; keep
it distinct from prior 15.7 ms NVIDIA stall and later owner query polling.
Native submission filters both fail closed (denied=0x12); do not bypass proof.
Preserve FPS/stereo/hands/input, use exact source contracts before any correction.
No additional runtime session authorized by the documentation-only workflow task.
