# Handoff: Zero-Latency Render Pipeline Optimization (SIDE)

**Date**: 2026-10-31
**Agent**: opencode (SIDE)
**Branch**: side/zero-latency-render-pipeline
**Worktree**: current (C:\Users\Trilex\Darkness2VR)
**Baseline**: origin/main (1051c5c6); branch created from side/producer-blocking-analysis HEAD

## Goal
Reduce D3D9/NVIDIA driver serialization and CPU frame time in SubmitMode 4 (same-frame stereo) by removing the render-thread wait on the left-eye producer D3D11 fence before right-eye capture, without adding frame latency.

## Status
Done (offline implementation + evidence; commit pending; MAIN validates on Steam)

## Context Loaded
- [x] BASELINE.md
- [x] FAILED-APPROACHES.md (checked: AFR rejected, single-surface shared capture is existing infrastructure)
- [x] Relevant evidence files: docs/research/zero-latency-render-pipeline-analysis.md

## Changes
- Files modified: src/d3d9.cpp (shared capture double-buffering: WaitSharedReader/EnsureSharedCapture/CaptureBackbufferToShared/ReleaseSharedCapture per eye slot; RenderEyePairFromReplay uses slot0=left, slot1=right; per-slot g_uploadSRV)
- Files added: docs/research/zero-latency-render-pipeline-analysis.md (dependency map + analysis + correctness)
- Commits: pending

## Evidence
- Addresses/offsets: src/d3d9.cpp lines ~2483-2579 (shared capture), ~2643-2781 (RenderEyePairFromReplay), ~3921-3919 (SubmitXR/WriteTextureToSwapchain), ~4515-4614 (VrFrame)
- Config: Darkness2VR.ini SubmitMode=4, StereoMode=1, ZeroCopy=1 (shared-capture path on critical path)
- Tests: offline MSVC x86 build of src/d3d9.cpp (compilation only); no Steam/runtime testing (SIDE constraint)

## Next Steps
1. MAIN reviews diff and builds via build.ps1
2. MAIN installs (Steam exclusive), in-game test SubmitMode=4/StereoMode=1: both eyes, no right-eye/hands/tentacles/UI regression, no added pose latency
3. MAIN confirms reduced render-thread wait via d3d9.log [StereoPerf]/retained ETW traces
4. MAIN merges or rolls back (d3d9.dll.off-<timestamp> backups)

## Blockers
None (offline only; runtime validation is MAIN's responsibility)

## Notes
- Deliverable framing: Removed dependency (right-eye capture no longer waits on left-eye producer fence within a frame) | CPU time saved offline (removed per-frame WaitSharedReader spin / worst-case render-thread stall; average small because GPU blit already overlaps CPU replay) | Correctness (per-slot fences, D3D11 in-order, no cross-thread D3D9, zero added latency) | Commit.
- If MAIN finds no measurable benefit, the next high-impact optimization is the right-eye ReplayStream cost (inherent to same-frame stereo; AFR previously rejected).
