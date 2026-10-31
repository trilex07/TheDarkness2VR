# Handoff: Four performance branches

**Date**: 2026-10-31
**Task ID**: four-branch-performance
**Role**: MAIN
**Branch**: codex/main-single-owner-d3d9-validation
**Worktree**: C:\Users\Trilex\.codex\worktrees\main-render-cpu-next\Darkness2VR
**Baseline**: accepted Steam DLL SHA256 34641A8981444EA05723E6F5F3D669099F6FECD3ABC1142ED1C686C6E42EC232; source f24ced92; Mode 4 retained.
**Integration commit**: resolve this document's introducing commit; deployment receipt records full SHA.

## Integration
- outdoor-recorder-lock 5b0d624b -> b32877d9. Fixed semaphore allocation failure by choosing an immutable SRW fallback backend. Eye parameter references retain frame ownership.
- present-overlap 42fe1592 -> 972a29eb. Fixed early-forward reentrancy and nested native-forward timing contamination. Adaptive policy can still increase latency; acceptance requires headset feedback.
- cpu-hotpath-opt c5b1a4e3 -> daafe24b; review f923c9c5 -> ad7cf0c6. Unrelated incident commit 297cb318 excluded. Exact split-slot color reuse preserves persistent right-eye history, COM references and reader fences.
- pipeline-variant-warmup 18d1a69c: original pointer/setter-shadow tracker remains rejected. Safe replacement samples actual device state at most once per frame, retaining indexed exact admission and asynchronous snapshot serialization from c7df7be3. No negative cache. Native opaque StateBlock changes reproduced and captured by the replacement. Stable draw counts have round-robin coverage; transient pipelines can be missed. PPW1 format, 4096-record limit and old learner remain compatible.
- All three new switches default OFF in source INI. Authorized manual Steam trial enables OverlapNativePresent, ReuseSharedCaptureColor and PipelineVariantLearning. Preserve Mode4, ConsumedRecordCopy=1, DirectRecordCopy=0 and ReplayBufferVersions=0.

## Offline evidence
- Recorder lock, forced allocation fallback, recursion and contention PASS.
- Present policy and actual native DLL entry: all eight observer modes PASS, including 5000-frame early forwarding/reentrancy and exact HRESULT/arguments.
- Actual D3D9/D3D11 split transfer plus actual EyeColorHistory: 2760 eye transfers, 1060 at 2676x2728, reader fences, persistent partial-frame contents, alias-detaching fallback and ResetEx PASS. REF and HAL history regressions PASS.
- Actual pipeline warmup REF regression PASS; opaque StateBlock variants, failed capture recovery, exact state bytes, round-robin budget and fairness PASS. Original source failure is retained separately.
- Actual-source native constants (413351 checks), projection history, replay surface status, shared slots, protected hands, independent left aim/throw, tentacles and native menu snapshot PASS.
- Python policy/source contracts: 55 tests PASS.
- Earlier sampler-caps-cache text matcher has a pre-existing stale Reset prefix failure; no runtime Reset defect established by that matcher.
- Logs: %TEMP%\four-*.log. Final x86 build identities and deployment backups live in %LOCALAPPDATA%\Darkness2VR\packages\four-branch-performance-20261031.

## Runtime gate
No FPS gain, hitch elimination or headset correctness claimed. User authorized deployment and will test manually; no game launch or dedicated profiling session. Compare both eyes/menu/pause, peripheral geometry, hands/tentacles, responsiveness and combat pacing. Restore package rollback/Restore.ps1 if rendering, latency or stability regresses. Retain the accepted backup and all caches.

## Deployment complete
Integration/source commit: 7159787160cf1702836ebd08de4594805421d460 (pushed).
Both final MSVC x86 builds PASS. Protected candidate DLL SHA256:
BFF8B0A9F5BE83C511762E66C78C5903BD9FB3A48FD3D2F81BF15C035FBD1CFA.
Deployed 2026-10-31; EXE unchanged, all nine rollback files verified, only DLL
and three intended Render settings in Steam/local INIs changed. Unmodified
profiles and caches retain their previous SHA. Full deployment.json, matching
DLL/PDB/MAP, 9-file rollback and four-*.log copies are in the named package.
Manual PSVR2 test READY; game not launched. New combined runtime results pending.

## Gameplay feedback / quarantine
User reports intermittent right-eye artifacts during gameplay (not menu).
Both new image-path changes were actually active: SharedCaptureColor active=1,
split=1, 2676x2728 format22; PresentOverlap issued 6879 early forwards by pair9600.
No causal attribution established. NativeSubmission remained denied=0x12;
no float setter failures/exceptions were reported in the retained upload counters.
Preserved runtime log and exact profiles at runtime-20261031T155300; preserved
before/after profiles plus learned caches at quarantine-20261031T155425 in the
package. At rest, changed ONLY OverlapNativePresent and ReuseSharedCaptureColor
to 0 in both primary Render INIs. Same deployed DLL, Mode4, ConsumedRecordCopy
and all other settings retained. No game launch or extra micro-test requested.
Combined image-path optimization acceptance is withheld. This quarantine is
not evidence that existing flicker was fixed or caused by either new switch.
