# Last-consumer shared reader fence — isolated candidate

Base: `c86c2c8`. Opt-in: `[Render] LastConsumerReaderFence=1` in the effective
render INI. Default remains 0; no INI/profile is changed by this commit.
This candidate is independent of `UniqueReplayResourceRefs` and its A/B test.

## One bounded hypothesis and correction

The legacy D3D11 event is issued by `WaitSharedReader` immediately before the
next D3D9 overwrite. It therefore includes commands queued after the shared
texture's last actual read, such as NIS over eye caches, final-image staging and
OpenXR eye-cache copies. The candidate issues that same event after the complete
shared-SRV Draw/unbind instead. Waiting before overwrite still flushes and polls
the already-issued event with the existing two-second timeout. No GPU operation,
producer fence, synchronization result check or eye is skipped.

This only narrows the dependency set, especially the last-eye to next-frame
transition. It does not remove GPU work, prove the dominant stall or establish
an FPS gain. Driver batching may limit the benefit. A later separately
authorized same-scene A/B must compare reader-fence wall time, total frame time,
correct stereo and stability; do not combine it with the ref-ledger experiment.

## Consumer/overwrite proof at this source revision

- `g_sharedTex11` is opened, used to create its SRV and released; no independent
  GPU read of that texture appears elsewhere.
- Exactly two scoped eye blocks alias `g_uploadSRV=g_sharedSRV11`, call
  `RenderStereoEyeD11` and restore the previous upload SRV.
- That routine's single Draw is the only GPU consumer and explicitly unbinds
  shared SRVs before sealing. Its earlier failure exits enqueue no shared Draw.
- NIS receives `g_eyeCache` textures, not the shared upload. Matched blit capture
  reads SRV descriptors only; final images and dumps read eye/NIS caches.
- `CaptureBackbufferToShared` still waits before its D3D9 StretchRect. Recording
  is sealed by existing `g_recActive=false` before internal capture; the
  StretchRect recorder tests that flag. There is no `g_internalStereoTransfer`
  symbol in this base. No second unguarded replay copy is introduced.

## Lifetime and failure behavior

The helper owns no COM references. It records only the current context/query
identity and thread; a different owner cannot poll or issue that query. A read
cannot begin with a pending query. Timeout or GetData failure leaves it pending;
only S_OK clears it. A rejected second Seal fails closed until resource release.

Resize still waits successfully before releasing old shared resources. Failed
resize leaves both resources and dependency intact. Reset/ResetEx preserve the
existing release behavior: old query/SRV/texture references are released, their
globals cleared, then helper identity resets. No old query is polled after
release, and a new texture/query lifetime cannot inherit its pending flag. A
failed device Reset does not recreate or write the old shared resource.
No new reset-time wait or cross-thread driver call is added.

Mode 0 preserves original End-at-overwrite, Flush, GetData polling, timeout and
return semantics; new reader bookkeeping is not consulted. Configuration is
read once by LoadConfig at process attach, not toggled during a resource lifetime.

## Verification and rollback

- Production and diagnostic x86 builds: PASS.
- Actual shared-reader helper regression: PASS for no reader, failed pre-Draw,
  unrelated late work, legacy ordering, repeated eyes, timeout/retry, driver
  failure/retry, pending-query reuse rejection, wrong context/query/thread,
  resize and Reset lifetime separation.
- Existing transfer-order regression extended with actual consumer/overwrite
  wiring checks: 5/5 PASS; both-eye anti-overwrite interleaving remains covered.
- Protected profile/startup regression: 3/3 PASS; integration profiles: 6/6 PASS.
- No Steam deployment or runtime/HMD test by SIDE. No FPS improvement claimed.

Rollback: leave LastConsumerReaderFence=0 (legacy) or revert this commit.
Persistent logging, protected Present, controls, head pose, shaders, resolution,
FOV, native hook policy and resource-ref option are unchanged.

MAIN integration: `e0d9fea` on `codex/protected-vr-integration` (from SIDE
`ad5f493`). MAIN reran the actual helper, all five transfer-order cases, all
13 protected-profile regressions, and diagnostic/production x86 builds: PASS.
Private diagnostic DLL SHA-256:
`ceb1cfa504e6f54246902b128f02acad73c4e9b17357fd737a96b84870870f7f`.
This fence candidate is NOT deployed in the resource-ref A/B, which remains
frozen at `c86c2c8` with the legacy reader fence in both variants. Runtime and
HMD benefit of the last-consumer candidate remain NOT RUN.
