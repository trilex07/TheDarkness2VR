# MAIN review: shared capture colour reuse

**Task ID**: shared-capture-color-reuse-review
**Date**: 2026-10-31
**Role**: MAIN (offline review only)
**Branch**: side/cpu-hotpath-opt
**Status**: REJECTED for enablement/merge pending combined-path runtime correctness evidence; default OFF retained. Reviewed source has no confirmed ownership defect.

## Exact ancestry and scope
Windows Git verified a clean worktree and the exact direct-parent chain:
`75432ae34353681b0a925364ce5f6fb6fca8e77b` ->
`c5b1a4e3d4cbee20325d87eb1d2560bb1e98b4f9` ->
`297cb31855dbe4f08a9ee2b8c5d7eec23e7315b1`.
`rev-list --left-right --count 75432ae3...297cb318` = `0 2`.
First commit is the nine-file reuse implementation; second only records the VM
Git incident. No cherry-pick/merge, branch replacement or worktree pruning.
Both refs resolve on Windows; MAIN's separate registration is restored.
Origin had no `side/cpu-hotpath-opt` head before this review's push.

## Deployed prerequisites: confirmed active, optimization not installed
Read-only installed identity: DLL SHA256
`34641A8981444EA05723E6F5F3D669099F6FECD3ABC1142ED1C686C6E42EC232`,
source f24ced92; Steam INI SHA256
`764EEADF2E5FB21E4F1A95B3884F39B8E75D3E94765CC10F74C9BCE5D5B8DF0B`;
local INI SHA256
`FE242875007DA0BFDCE05B99A15256A22256550609A934CB4BA2BE18E0D17020`.
The retained d3d9.log confirms requested/admitted mode=4, native flags=0x50,
deferReader=1, lastConsumerFence=1 and `[SharedCapture] eyeSlots=2`.
Thus split capture was actually running, not merely requested in a repository INI.
No ReuseSharedCaptureColor log exists in that older deployed binary.
The new switch is absent from installed profiles and defaults to zero in this branch.
Nothing was deployed or launched during review; installed hashes remain unchanged.

## Ownership, fences and persistent history review
- Slot zero is written by left capture; slot one by right capture on the split path.
  Pair serial, exact RT identity, size, single-sample X8/A8 checks guard reuse.
  Mirror is cleared before a new capture attempt. Validation also requires the
  outer successful capture result, so a later producer failure is not admitted
  merely because StretchRect had succeeded.
- Adopt retains both target and copy with AddRef before releasing replaced refs.
  Save detaches adopted storage before writing; Restore only reads that storage.
  Seed precedes the following slot-one overwrite on the same D3D9 owner queue.
  This persistent read dependency is D3D9 ordered, not protected by the D3D11
  reader query; changing that ordering would invalidate this optimization.
- Existing exact per-slot D3D11 reader fence still gates every overwrite.
  Worker packets retain SRV/query/context resources; modes 3/4 keep D3D9 queries
  on the owner. No new worker writes, context users, synchronization or disk I/O.
- Successful right capture adopts only this pair's exact copy. Failed/nonmirroring
  capture saves RT0 before restoring live colour. Rejected replay invalidates
  history; fallback Save cannot write through a D3D11 input alias.
- Resize can release slot ownership while history's own COM reference keeps the
  old surface alive. A changed output identity discards that old history.
  Reset/ResetEx release history before shared slots; outstanding failed reader
  dependencies retain resources and refuse Reset. No Reset ordering change.

## Validation findings / acceptance blockers
1. The supplied HAL history fixture did not exercise Adopt at all. This review
   extends the existing actual-source REF/HAL fixture with adoption, queued
   seed-before-overwrite (no intervening readback/fence), fallback detachment,
   rejected history and release before three Reset cycles. PASS on both drivers.
2. The transfer fixture separately exercises real D3D9Ex/D3D11 sharing and mode 4,
   but does not combine the production pair scheduler's persistent adopted
   history with those readers and failure/resize transitions. Neither it nor
   the history fixture proves the complete new path in a running VR game.
   Consequently enablement/merge is rejected until that combined correctness
   gate is validated. Passing source/model tests is not a headset proof.
3. No measured improvement for this candidate. The handoff's 0.12-0.26 ms copy
   bandwidth estimate is theoretical, not recoverable CPU time. A 1.4 ms join
   cannot be attributed entirely to two copies; GPU overlap and queue dependencies
   prevent inferring a one-for-one gain from inclusive timing. Retained fallback
   allocations also mean a universal two-surface VRAM saving is not proven.

## Windows MSVC x86 results
- Release and DiagnosticCapture builds PASS; no Install.
- shared_color_reuse: 402242 checks PASS.
- actual-source history, shared_capture_slots (23182 checks), real
  stereo_transfer_worker (2660 transfers; 1040 at 2676x2728), state_snapshot,
  pre_scene_clear, replay_surface_status, frame_resource_refs, swapchain_write:
  all PASS. Both original and extended REF/HAL history fixtures PASS.
- Existing Python tools/test_*.py: 51 tests PASS. diff --check PASS.
- No new failures found; no game/OpenXR validation or FPS claim.

Local logs and frozen release DLL:
`%LOCALAPPDATA%/Darkness2VR/reviews/cpu-hotpath-20261031/`.
Release DLL SHA256 `13707683CDBAC9FD8050568CEFD8127075EDE30661F990F488881BBFE9DBA05D`;
diagnostic DLL SHA256 `2D5EE743FDFAC95B28AA920DD0F928640EFE78A89EF732F725853B781A7F63EA`.
These binaries match reviewed renderer source; this review changes tests/docs only.

## Next gate
Keep ReuseSharedCaptureColor=0 and the working Steam baseline intact.
Do not merge or enable automatically. A later explicitly authorized consolidated
validation must cover both-eye persistent UI/world history, transfers, Reset and
frame pacing together; quantify actual saving before acceptance.
