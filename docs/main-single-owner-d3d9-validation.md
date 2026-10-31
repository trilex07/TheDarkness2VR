# MAIN single-owner D3D9 validation candidate

Integrated only the mode-4 implementation/config comments and transfer tests from
`25c630921dfb5024394ec4e3880ae6634d5ff7f5`. Base:
`126755e04fbac7813636e679e4fb42ec6662f173`. No unrelated branch history merged.

## Review and integration

- Mode 4 retains the exact issued/flushed D3D9 event on the render thread. Polls
  after later D3D9 work use flags zero. Only S_OK permits publication of a
  D3D11-only packet; no D3D9 query or COM reference is owned by that worker job.
- Staged early returns retire the owner query without submitting D3D11. Failure
  latches transfer rejection; submitted jobs retain resources until the exact
  ticket completes. Deferred D3D11 readers remain fenced before overwrite/Reset.
- Existing two-slot fallback and Reset/ResetEx lifetime contracts are retained.
  Device creation adds MULTITHREADED only in modes 1/2. Mode 4 preserves native
  flags; the hardware fixture verifies 0x50, but the real game must verify it too.
- MAIN reconciliation preserves the original mode-1 join-before-history ordering.
  Only mode 4 extends overlap through the D3D9 right-eye history copy. Existing
  mainJoinMean includes residual mode-4 owner waiting as well as worker join;
  workerMean and StereoPerf remain overlapping intervals, never additive costs.
- Native admission hooks remain after shader pool and pipeline warmup restoration.
  ConsumedRecordCopy, warmup, native FOV, replay version quarantine, hands,
  tentacles, input and OpenXR are unchanged. No EXE instructions modified.

## Offline results

Both optimized x86 builds PASS. Protected diagnostic candidate DLL SHA-256:
`34641A8981444EA05723E6F5F3D669099F6FECD3ABC1142ED1C686C6E42EC232`.
Ordinary DLL SHA-256:
`5CD4DBBB8E51E07F39BE88283197955D58560AB6065331F515125756C0BB1E21`.

19 regression suites PASS: stereo_transfer_worker, shared_capture_slots,
render_command_worker, submission_epoch, native_submission_cache,
native_submission_boundaries, pipeline_warmup, replay_buffer_versions,
replay_surface_status, live_stateblock_sync, stateblock_recording,
projection_history, frame_resource_refs, pre_scene_clear, tentacle_head_scope,
replay_binding_cache, protected_hands, protected_left_aim,
native_menu_input_snapshot.

The real x86 HAL D3D9Ex/D3D11 fixture completed 2660 eye transfers, including
1040 at 2676x2728. It verifies pixel equality, native 0x50 / MT 0x54 devices,
actual Stage/Poll/Promote, rejected duplicate publication, staged destruction,
independent shared inputs, explicit reader retirement, failure latching and
ResetEx/recreation. These are isolated hardware tests, not Steam/PSVR2 acceptance.
No pre-existing regression failure encountered in these 19 suites.

## Preserved installation and comparison gate

Local private package (DLL/PDB/MAP, logs, identities, both profiles, rollback):
`%LOCALAPPDATA%/Darkness2VR/packages/single-owner-d3d9-20261031`.
Rollback snapshots the current installed DLL SHA
`416F710996324C7572863F122E1A1F842A3DFEBE5D3316D055A5A857AA5F8F14`,
all four INIs and all four warmup/shader caches; every copy is SHA-256 verified.
`rollback/Restore.ps1` restores exact files with the game closed. The older
physically accepted 4a26aabd rollback remains separately preserved.

Current integration base includes native submission/binding filters whose
headset acceptance is still pending. Preserve their installed settings equally
in both runs; do not label that untested candidate a newly accepted baseline.
Mode-1 profiles are byte-identical to the installed primary INIs; mode-4 profiles
change only StereoTransferWorker=1 to 4. ConsumedRecordCopy=1,
StartupShaderPool=1, PipelineWarmup=1, DirectRecordCopy=0 and
ReplayBufferVersions=0 remain unchanged. Source defaults remain unchanged.

Steam files have not been modified for this task. No game launched.

At the next authorized headset session, use the same protected candidate DLL
for both modes, the same saved scene, resolution, route and cache snapshot.
Preserve learned caches/logs before restoring the identical startup cache for
the second launch. Prepare any PresentMon elevation before starting the game.
Use existing PresentMon/QPC analysis, StereoPerf, transfer logs, PerfClock and
headset feedback; no WPR CPU.Verbose campaign. Measure 5 s stationary + 25 s
combat per mode. Keep each process alive until logs/history are preserved.
Verify actual creation flags (mode 1 expected 0x54; mode 4 expected 0x50),
both eyes/transfer completion, tracking, hands, tentacles, geometry and latency.
Capture FrameTime/CPU metrics, p95/p99 and transfer timings over matched phases.
PresentMon CPU metrics are not CPU-Precise RUN/READY/WAIT or HMD display FPS.

Mode 1 vs 4 CPU/frame: NOT RUN. p95/p99: NOT RUN. Visual: NOT RUN.
Decision: candidate only, not accepted. No claimed frame-time saving.
The separate 15.7 ms NVIDIA producer-flush dependency is not attributed to this
worker and is not claimed fixed. Restore the saved working DLL/INIs on regression.
