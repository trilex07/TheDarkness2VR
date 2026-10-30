# Consolidated native CPU candidate; runtime not yet measured

The accepted ConsumedRecordCopy baseline is preserved. This offline integration combines release batching (2f24b31e), final-owned float submission elision, frame-owned VS/PS/geometry binding elision and the 4096-record startup warmup correction. NativeSubmissionFilter and NativeBindingFilter remain independently default OFF; the binding filter requires the submission ownership gate. DirectRecordCopy and ReplayBufferVersions remain quarantined OFF. The final diagnostic candidate is now installed for a later manual PSVR2 test; the game was not launched.

## Integration defect fixed

The first consolidated warmup regression failed: HookDevice installed the native admission QI/SWVP vtable hooks before startup pool/warmup. Move those hooks after StartupPipelineWarmup and its restoration, still before the unpublished device is returned to the native producer. Device identity is published before installing the admission hooks. Existing actual-source ordering regression is FAIL before / PASS after; the actual x86 REF warmup/restoration/Reset regression also passes. No bypass exemption for driver callbacks or escaped interfaces was added.

## Scale and scope

Reused complete capture 20261030T221816Z-61b77f88 (1528 combat application intervals): exact named float setters expose approximately 1.408 ms/frame, six shader/geometry binding setters 0.420 ms/frame. These are disjoint classified sampled exposure, including required work, not predicted savings. The elision candidate cannot remove upstream native processing or all recorder transformations. Additional scalar setters expose only 0.270 ms/frame including necessary work: no new scalar cache is implemented. Native addresses from the closed resource-demand cycle are not investigated again.

The NVIDIA worker-to-render semaphore dependency remains proven at 15.663 ms WAIT plus 0.009 ms READY in a 15.7137 ms producer flush. The compiler contributes only 22 sampled ms throughout combat, so eliminating this occasional hitch cannot recover whole sustained milliseconds per frame. Learned/warmed startup counts are verified separately in nvidia-warmup-validation-20261030.md. That runtime result does not validate this native filtering candidate.

## Validation boundary

Twenty relevant native/recorder suites passed before the ordering correction; the targeted warmup regression passed after it. Both corrected x86 DLL builds and all 21 affected suites PASS at source 22920d5b. The private native-cpu-consolidated-20261030 package freezes DLL/PDB/MAP, per-suite SHA/logs, the failing-before receipt, an ON test-profile copy and nine hash-verified predeployment rollback files. Steam was updated only after the complete offline gate, broad game-session check, EXE/current DLL/INI/cache hash checks and rollback verification. The earlier CPU snapshot fixture had an environment execution denial; no snapshot PASS or headset acceptance is inferred. Runtime admission, actual skipped calls, frame-time gain and both-eye correctness remain NOT RUN for this native filtering candidate. Existing sparse NativeSubmission/NativeBinding counters must show admission and real skips before any performance claim. No extra ETW capture or historical baseline is needed to prepare it.


## Rollback-protected deployment

Installed protected diagnostic DLL SHA-256: 416F710996324C7572863F122E1A1F842A3DFEBE5D3316D055A5A857AA5F8F14. Built source 22920d5b; integration 47dc89cc is code-identical. Effective primary Render settings verified through the existing Windows INI reader: NativeSubmissionFilter=1, NativeBindingFilter=1, ConsumedRecordCopy=1, StartupShaderPool=1, PipelineWarmup=1, PipelineWarmupMs=30000; DirectRecordCopy=0 and ReplayBufferVersions=0. The current actual SamplerCapsCache=1 setting was preserved (the earlier context note saying 0 was stale). Only DLL and the two primary Render INIs changed; diagnostic/input profile, last-lighting INI, all learned caches, game EXE, localization and SteamVR settings remain byte-identical to the preserved preimage.

Nine predeployment files and Restore.ps1 remain in the private native-cpu-consolidated-20261030/rollback package; the separately retained accepted 4a26aabd rollback remains available too. No game launch or additional capture. Runtime admission/skips, native CPU saving, visual correctness and 90 Hz are NOT YET VERIFIED. Use one later consolidated manual gameplay check with existing sparse counters; do not repeat a heavy collector that the user reports costs about 20 FPS.
