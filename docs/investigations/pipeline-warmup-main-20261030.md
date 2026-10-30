# MAIN PipelineWarmup integration — 2026-10-30

Status: integrated and offline validated; Steam deployment and two-launch PSVR2 validation are pending authorization. No hitch reduction or FPS improvement is claimed.

## Source and baseline

Integrated `84c633e216d81c78c10c43fbf65be0348d8c465e` from `perf/producer-flush-compiler-stall`, with decoder prerequisite `d81b3e27`, onto MAIN `fef3a767`. Local integration commits: `1bb9588a`, `b25b63f4`, followed by the restoration corrections in this commit. No automatic merge.

The installed baseline remains diagnostic `5ff1b7dd`, SHA-256 `4FB4E4B04A57EA4B411936C0AADE9F46CEDFB6129FA062157DD9A2ACDE18853B`. The retained zero-loss combat capture `20261030T155718Z-497ee850` contains a 21.2575 ms producer-flush GetData call, a 21.215 ms off-CPU interval and NVIDIA compiler-worker dependency. The retained ring alone does not prove the lifetime-first use of a particular shader or exclude later pipeline variants. See `compiler-dependency-20261030.md`.

## Integration review and corrections

- Startup uses pending pool-owned objects with exact bytecode and device identity, before vtable patching and engine device handoff. Warmup borrows references; it neither creates replacement shaders nor consumes pool ownership. Every available pooled copy is submitted. Private DEFAULT-pool buffers, surfaces, textures, declarations and query references are released before Reset/ResetEx. The existing shader-pool Reset protocol is preserved.
- The original restoration dropped MRT1–3 because D3DSBT_ALL does not capture render targets. Added explicit, capability-bounded capture/restoration of every supported target and depth, with state-block Apply last to restore the viewport. A real D3D9 regression failed before and passed after this correction.
- The original format-only surface cache aliased equal-format MRT slots, causing actual D3D9 Prepare to reject an otherwise supported pipeline. Keyed private surfaces by slot as well. A second regression failed before and passed after this correction.
- Exception unwinding now restores saved state before releasing private resources. Temporary COM ownership protects successful resource creation across vector allocation failures. Failed state queries, buffer Unlock and EndScene are checked; absent depth/unused targets remain supported.
- A missing required texture or depth resource rejects that warmup record rather than counting a different null-resource pipeline as warmed. The inherited `substitutedTextures` field now counts unavailable texture creations, whose records are skipped.
- Degenerate indexed triangles draw only into private 16x16 targets. Completion is a startup EVENT query, not new gameplay synchronization. `driverComplete=1` proves queue completion, not elimination of NVIDIA first-use compilation. There is no guarantee that non-rasterizing draws warm every future driver specialization.
- Learning remains bounded to first observed shader-object pairs on the producer thread, with fixed pipeline/declaration limits and asynchronous manifest persistence. Repeated variants of one pair are not exhaustively learned. No new profiler, per-draw disk logging or native EXE instruction modification.

## Offline verification

PASS: both final x86 DLL builds, ordinary and DiagnosticCapture.

PASS: actual D3D9 REF and HAL/Ex (creation flags 0x54) warmup tests: manifest admission/dedupe/round trip, null PS, every pending copy, ownership, unavailable/unsupported records, failure stop/budget, explicit MRT restoration, distinct same-format MRTs, exception restoration, Reset and ResetEx. Pool identity and returned shader ownership remain intact.

PASS: shader preload pool; actual StateBlock construction and live state-block sync; shader/projection metadata and inherited WVP; four live draw wrappers; recorder/live HRESULT receipts; VB/IB consistency and reentrant callbacks including Reset; replay state/surface receipts; binding filter and constant reuse; protected XInput; bounded command history and its 7 decoder tests; 6 PresentMon analysis tests and 31 real collector-helper checks. An exploratory unittest discovery invocation lacked the required binary-fixture argument; the supported command-history runner and explicit fixture invocation both passed. No product regression attributed to that invocation error.

## Private candidate and rollback

Package: `%LOCALAPPDATA%\Darkness2VR\packages\pipeline-warmup-main-20261030`.

`ordinary/` and `diagnostic/` contain matching DLL/PDB/MAP. `rollback/` contains the verified installed DLL, four INIs, SHA manifest and `Restore.ps1`. `candidate-config/` contains the same accepted settings with only `StartupShaderPool=1` and `PipelineWarmup=1` enabled in both current and fallback rendering profiles. DirectRecordCopy and ReplayBufferVersions remain OFF. Native FOV, stereo, hands, tentacles, input, OpenXR and accepted CPU switches are preserved. Installed EXE identity and every existing DLL/INI SHA match the previous deployment receipt. Steam is unchanged.

## Authorized consolidated validation

1. Recheck session availability and all staged/rollback hashes. Deploy diagnostic candidate and the prepared INIs; preserve the matching symbols and command-history path. Do not start WPR or another game while the user is in the headset.
2. First launch: verify correct stereo/startup; load the same outdoor combat scene and play it to learn real gameplay pipelines. Reuse existing lightweight performance counters. Keep the game alive and minimized until manifest persistence/history export is confirmed. Parse and preserve the gameplay manifest with its SHA and counts before closing the process.
3. Second launch: require `warmed > 0`, `driverComplete=1`, `restored=1`; record unsupported/missing/time-budget counters as well. Return to that same combat scene. Use the existing command history and one existing lightweight collector session if needed; preserve matching DLL/PDB/MAP, INIs and QPC evidence. Keep the game alive until export is verified.
4. Compare producer-flush dependency stalls and unique application frame-time p95/p99, accounting for collector overhead. Compare like measured quantities; do not use compositor-generated frames as application FPS. Check both eyes, slow head movement, peripheral geometry, shadows, hands, tentacles, input and frame pacing together.
5. Restore the verified rollback upon startup/Reset failure, corruption, instability or a material performance regression. Retain failing evidence. Leave right-eye correctness and hitch elimination unverified until headset/runtime results exist.

Runtime result: Learned NOT RUN | Warmed NOT RUN | Stall before 21.2575 ms observed dependency / after NOT RUN | FPS/p95/p99 NOT RUN.
