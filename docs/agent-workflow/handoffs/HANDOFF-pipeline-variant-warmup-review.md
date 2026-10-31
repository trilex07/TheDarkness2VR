# Pipeline variant warmup review (2026-10-31)

- **Branch:** codex/main-single-owner-d3d9-validation
- **Verdict:** REJECT `18d1a69cb3950e7840fc957707aa347f0e3cd03a` as a runtime integration. Keep the independently tested manifest improvements below.
- **Source:** side/pipeline-variant-warmup, pushed from Windows and remote SHA verified. Parent/merge-base: `75432ae34353681b0a925364ce5f6fb6fca8e77b`.
- **MAIN before review:** `d08dcfb4e1176d3a5c079dd4f318cd90a53f1572`. Candidate cherry-pick `4af870f1`, explicitly reverted by `8ec39b3c`. No unrelated history merged.

## Blocking findings in the rejected commit

1. **P1: Opaque state changes can permanently hide a variant.** `ObserveDraw` consults `live.Key(v,p)`/`lastKey`/`seen` before `Capture`. `Hook_SBApply` synchronizes shader identities, but does not update or invalidate the learner signature. Native Reset/ResetEx and owner changes also lack a complete tracked-state resynchronization. All State fields have nominal signature slots (declaration, four RT formats, depth, five render states, four frequencies, sixteen texture classes/sRGB), but slot presence does not prove correspondence to the device at a draw.
   - Reproduced on actual x86 D3D9 REF: capture a D3DSBT_ALL with ALPHAFUNC=LESS; restore GREATER and notify TrackRenderState; observe; apply LESS through the native block; observe again. Expected an additional record; `variants.manifest.Size()==beforeOpaque+1` FAILS. This is a learning miss, not proof of visible corruption.
   - Reproduce with `test/review_pipeline_variant_stateblock.ps1`. It archives the pinned source into TEMP, compiles the original fixture plus this assertion and requires the specific expected failure. It does not modify the checkout, Steam, or installed caches.
2. **P1: Resource-class cache has no lifetime/generation contract.** `ResourceClass` indexes raw COM addresses with no owning reference or destruction notification. After resource destruction/address reuse it can return the previous object's format/type/usage/declaration. Failed descriptor queries also cache class 1. A real-device Capture only verifies keys that reach Capture; it cannot recover a variant suppressed by a stale key. Clearing at 32768 entries is a memory bound, not invalidation proof.
3. **P2: Discovery has a count limit, not a CPU-time limit.** Sixteen captures/frame each query shaders, declaration, caps, RT/depth, render state, frequencies and up to sixteen textures/samplers. The initial unseen-frame cost can be substantial; new resource descriptor queries and unordered-map lookups also occur in setter hooks. The key is 64-bit heuristic identity, not collision-proof completeness. The 65536-key ceiling, pointer reuse, and two-failure suppression can stop future discovery. No per-draw time or hitch-reduction claim is supported by these fixtures.

## Concrete corrections retained on MAIN

- `Manifest::Admit`: replace a full-record linear scan with a bounded hash multimap. Hash covers shader lengths, exact VS/PS words and all serialized State bytes. Every bucket hit still compares the complete Record, so hash collisions cannot admit the wrong record or reject a distinct one. Expected work is O(record bytes + collision bucket), not worst-case O(1). Failed index insertion rolls back the appended record/version.
- `Tick`: remove full archive serialization from the Present thread. The single existing writer now takes an immutable record-pointer snapshot with matching count/version under the mutex, then serializes and writes after releasing it. No driver calls or COM objects are moved to the worker. Present still performs the existing job allocation/thread startup/logging and short manifest locks; no zero-overhead claim.
- Record snapshots own const Records, so concurrent append cannot invalidate data in an in-progress write. Only one writer is admitted; failure clears its gate and leaves the dirty version eligible for retry. Learner lifetime remains the existing process-lifetime contract.
- PPW1 Magic, StateWords, field layout, exact admission, record ordering, 4096 records and 32 MiB serialized-size cap are unchanged. Existing 1024-record manifests remain readable. No capacity increase or eviction policy. Full manifests still stop learning; pipelines omitted after saturation compile normally in gameplay.
- Existing startup variant replay is unchanged: private resources, pooled shader ownership, Prepare/Draw, completion and Restore/Reset contracts. This review does not establish that the captured subset covers all NVIDIA specialization inputs (e.g. vertex samplers and many raster/depth/blend states are outside the format).

## Verification

- Original candidate: both MSVC x86 builds (standard and DiagnosticCapture) PASS; pipeline warmup actual-source fixture and four Python source contracts PASS. Additional actual D3D9 StateBlock probe FAIL as described above; that is why passing pre-existing tests was insufficient.
- Final corrections: both MSVC x86 builds PASS. Six runners PASS: pipeline_warmup, live_record_status, live_resource_binding, live_stateblock_sync, stateblock_recording, stereo_transfer_worker.
- Warmup runner includes exact admission/roundtrip, old 1024/full 4096 manifests, rejection at capacity, concurrent append/serialization of coherent prefixes, failed-write retry, capture failures, actual D3D9 REF warm/restore and resources released before Reset. Source guard requires Serialize in WriteManifest, absent from Tick.
- `tools/stereo-math-tests/build-sampler-caps-cache.ps1`: PRE-EXISTING FAIL at its exact-text Reset-prefix assertion. Reproduced on candidate and final code. `src/d3d9.cpp` and `src/stereo_replay.inl` have zero final diff versus pre-review d08dcfb4; their Reset/sampler behavior was not changed by retained fixes. This runner did not reach compilation; do not report every regression green.
- Private logs and SHA-256 inventory: `%LOCALAPPDATA%/Darkness2VR/packages/pipeline-variant-review-20261031/`. Includes both candidate and final build logs, expected-failure output and six final regression logs.

## Preserved Steam state and next gate

No deployment, game launch, collector, profiling session or INI change. Installed DLL remains SHA-256 `34641A8981444EA05723E6F5F3D669099F6FECD3ABC1142ED1C686C6E42EC232` (source f24ced92). Steam INI `764EEADF2E5FB21E4F1A95B3884F39B8E75D3E94765CC10F74C9BCE5D5B8DF0B`; local INI `FE242875007DA0BFDCE05B99A15256A22256550609A934CB4BA2BE18E0D17020`. Effective Mode 4, ConsumedRecordCopy=1, PipelineWarmup=1 and StartupShaderPool=1 preserved.

Rollback package unchanged: `%LOCALAPPDATA%/Darkness2VR/packages/single-owner-d3d9-20261031/rollback`.

The rejected tracker needs lifecycle-safe class identity plus complete resynchronization for opaque/native state restoration before reconsideration; querying Capture after a deduplication miss is insufficient. Any later replacement also needs a defensible discovery-time budget. No dedicated Steam test is requested. The independent manifest fixes are offline validated only; physical VR correctness, actual CPU savings, 90 FPS and hitch elimination remain unverified.
