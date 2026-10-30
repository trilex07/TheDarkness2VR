# Gameplay warmup rejection: INTZ depth ownership

## Runtime evidence

The first authorized launch learned 407 valid pipelines and persisted the manifest before the user closed the game. The actual-source C++ parser admitted all 407 records; 399 include depth and all 407 use textures. The manifest SHA-256 is `D1DCCF02D563D1ED46A53332FC5A1F89DB3C05F4C4AAB806676DB8700C8F2DBF`.

The second launch used deployed commit `54f3502125f7897a9b96d905e004baf20a7063fb`, DLL SHA `289EEA1C83D6FC4816BE29B0AE891E970FC21F4B094C53BF41009986D75FAC4A`. Its actual startup report was:

`records=407 warmed=16 draws=35 missingObjects=0 unsupported=391 substitutedTextures=0 timedOut=0 failed=0 driverComplete=1 restored=1 elapsedMs=31`

Exactly 391 saved records have INTZ depth format `0x5A544E49`; the other sixteen use ordinary D24S8 or no depth. The second session expanded the saved manifest to 621 records, including 604 INTZ records. Evidence is retained in the private `pipeline-warmup-main-20261030/launch1` and `launch2` package directories. No game-derived shader bytecode is committed.

The user reported finishing rather than readiness. The prepared collector remained `READY_WAITING_FOR_SCENE`; no ETW/PresentMon capture started. Its absence is explicit: no combat p95/p99, compiler attribution or comparable FPS result exists for this session. An early lightweight producer-fence maximum was 18.858 ms, which neither identifies its compiler cause nor proves a reduction against the previous 21.2575 ms captured dependency. Do not retrospectively capture the minimized game as combat. After evidence preservation, MAIN closed the authorized game PID 9456.

## Exact source defect and correction

`d2vr_pipeline_warmup::DeviceWarm::Surface` used `CreateDepthStencilSurface` for every depth format. On the actual x86 D3D9 HAL/Ex driver with creation flags 0x54, INTZ returned `0x8876086C` (D3DERR_INVALIDCALL), while `CreateTexture(..., D3DUSAGE_DEPTHSTENCIL, INTZ, D3DPOOL_DEFAULT)` returned S_OK. The texture-backed resource's level-zero surface is the required API contract.

The correction creates only INTZ as a private depth texture, retains both its texture owner and level-zero surface through the warmup, restores previous bindings, then releases both before Reset/ResetEx. Ordinary depth formats retain their existing path. No format substitution, fallback to null depth, new gameplay synchronization, changed EXE instructions or changes to shader-pool ownership.

## Verification

- New actual HAL/Ex regression: FAIL before / PASS after. Covers INTZ depth and sampled INTZ, exact format, state restoration and ResetEx after private-owner release.
- Existing actual REF test: PASS, including MRT isolation/restoration, exception cleanup and Reset.
- Existing fixture accepts an optional private gameplay manifest (`-Hardware -GameplayManifest <path>`), parses it using the actual source and warms its exact shader bytecode/state with borrowed pool objects. No profiler or game launch is added.
- First gameplay manifest: `records=407 warmed=407 draws=814 unsupported=0 missing=0 textureFailures=0 timedOut=0 failed=0 driverComplete=1 restored=1`.
- Expanded second gameplay manifest: `records=621 warmed=621 draws=1242 unsupported=0 missing=0 textureFailures=0 timedOut=0 failed=0 driverComplete=1 restored=1`.
- Both actual-manifest cases preserve pool ownership and successfully ResetEx after cleanup. These are isolated-driver functional results, not Steam/headset FPS or hitch-reduction proof.
- Ordinary and DiagnosticCapture x86 DLLs and matched symbols are frozen in the private `pipeline-warmup-intz-20261030` package; build results and hashes are recorded there after completion.

## Next gate

Preserve the 54f35021 rollback and accepted rendering/input settings. Keep StartupShaderPool and PipelineWarmup ON, DirectRecordCopy and ReplayBufferVersions OFF. Reuse the learned gameplay manifest; another learning launch is unnecessary. A future consolidated warm launch must verify actual startup coverage, stereo/depth/peripheral correctness and measured outdoor stalls. Require readiness BEFORE starting the existing collector; retain the game until history export is verified. No hitch-elimination claim until that runtime evidence exists.
