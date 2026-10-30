# Replay binding filtering - offline validation

Base: latest committed MAIN 7371fbe6 plus SIDE CPU reductions 18006227.

Texture, VS/PS, declaration, stream (including offset/stride), instancing frequency and index bindings previously entered the driver on every recorded setter. Replay now filters exact successful repeats inside one replay only. Shader metadata selection runs only on an actual successful shader change. The existing command/snapshot owners retain identities. Cache size is fixed; no extra resource retention, allocations, getters or synchronization.

State blocks and caught driver exceptions clear all binding knowledge. FVF invalidates declarations; UP draws invalidate stream zero; indexed UP also invalidates indices. Vendor/unsupported render state commands conservatively clear binding knowledge. Every VB/IB packet still resolves and validates its buffer version before filtering. Native calls, payload copies, snapshots, FOV/tracking, draws and replay ordering remain intact.

Actual-source x86 regression compares every binding at 4,000 simulated draws, including SB/FVF/UP side effects, instancing, offsets and vertex textures: 64,320 to 6,919 binding calls (-89.2%), 8,218 to 436 metadata lookups (-94.7%). CPU-only mock 0.233 to 0.209 ms/frame; real driver/GPU excluded. Also checks failed setters, retry, nulls, unknown slots, new replay reset and invalid buffers despite identical handles. This synthetic material workload does not establish the real game's repetition rate or FPS gain.

No Steam deployment or runtime performance claim. MAIN owns PSVR2 acceptance. Existing historical ETW predates current native FOV and buffer versioning; shader creation, traversal and transfer waits remain current-runtime measurement gaps.

Recovery integration: MAIN 213bb4f7 was committed during this SIDE run, after a user-reported runtime hang/CPU regression with the buffer candidate (cause not isolated). This branch includes that recovery, keeps ReplayBufferVersions=0 and ReplayStateFilter=0, and introduces separate startup ReplayBindingFilter=0. Explicit opt-in is required to exercise binding filtering. Offline gains are candidate measurements; no runtime gains or resolution of the hang are claimed.

Final recovery-compatible validation: both x86 DLL configurations PASS; actual binding filter OFF/ON, live hooks, buffer versions/disabled bypass, replay state/surface/float, UP geometry and right-eye history PASS. Reference lifetimes, small/direct record copy guard regressions, 1,206 rendered-FOV rays, native camera boundary and binocular culling regressions PASS. Earlier unchanged shader history, metadata and tentacle suites also passed. Snapshot CPU runner is BLOCKED: normal build/run reported Access denied; one retry at the same output path failed LNK1104 opening state_snapshot_receipt_test.exe. The executable is absent, directory ACL allows writes, no matching running test process was found. The enforcement/lock owner is unknown; Defender threat-query access was denied. No path rename, security exclusion or alternate execution method was attempted. This auxiliary test is not claimed PASS. No Steam, deployment, MAIN worktree edit or runtime FPS result.


## MAIN integration on accepted protected-native base

Integrated 38fe1fee, e6268ae6 and f2a3c4f3 onto accepted MAIN 1dd6205e in
codex/main-render-cpu-next. Existing MAIN worktree has uncommitted menu/font work;
it remains untouched. Recovery code already present in 213bb4f7 was reconciled,
not duplicated. The build-flavor guard and protected-native marker are retained.

Bindings reset at replay entry, StateBlock Apply and caught exceptions; FVF,
UP/DIPUP and vendor state paths invalidate their affected knowledge. Only exact
successful bindings are skipped, with VB/IB validation preceding the filter.
Metadata memoization is fixed at 128 slots (1024 bytes on x86), pins source
interfaces, rechecks mutable validity/locks/aliases and drains pins at replay end
and Reset. No new lock or native draw-lock scope is introduced. With
ReplayBufferVersions=0, metadata lookup/cache and versioning remain bypassed.

Validation on the integrated tree:
- x86 standard and protected-native DLL builds PASS. 44 actual-binary flavor
  checks PASS; the wrong variant is refused before installation writes.
- Actual binding fixture: identical state at 4000 draws; 64320 -> 6919 setter
  calls and 8218 -> 436 shader metadata selections in its synthetic workload.
- Six original VB/IB failure cases, range uploads, NOOVERWRITE, quota, failed
  calls, Reset, worker uploads, disabled bypass, metadata pin/collision/teardown
  checks PASS. Lookup fixture: 4000 -> 1 private-data calls, zero allocations.
- Actual-source replay state/surface receipts, shader metadata, right-eye color
  history, projection history (145248 checks), live state blocks (296163), live
  hooks (5688), UP geometry (4403), and both float-loop build flavors (58510
  checks each) PASS.
- CPU snapshot receipt fixture compiles, but execution returns Access denied.
  This remains BLOCKED, not PASS. Its DeviceSnapshot implementation and fixture
  are byte-equivalent to accepted 1dd6205e. No security-control bypass attempted.

ReplayBindingFilter=0 is independent. ReplayStateFilter=0 and quarantined
ReplayBufferVersions=0 remain OFF. No changes to DirectRecordCopy, FOV, hands,
tentacles, input, haptics, OpenXR, localization or Steam files. The protected-native
candidate is staged privately for the next consolidated MAIN validation; no
installation or launch during the user's Polish-font test. The user reports
accepted 1dd6205e no longer crashes and FPS returned to its previous level;
this is baseline feedback, not a result for the new integrations. Runtime gain,
frame-time regression and visuals for this candidate remain NOT RUN.
