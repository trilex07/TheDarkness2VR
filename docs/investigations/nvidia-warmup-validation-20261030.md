# MAIN NVIDIA warmup validation candidate

Integrated source 73a461b0 onto exact headset-accepted ConsumedRecordCopy baseline 4a26aabd. Separate branch codex/main-nvidia-warmup-validation; independent native submission/binding candidates are excluded. Only the intended warmup source changes were picked. Documentation conflict resolved without importing the unvalidated reference-release optimization.

Changes: 4096-record / 32 MiB backwards-compatible manifest; one retry of a failed first capture; covering startup geometry with real indices, per-stream storage and identity constants. Preserved recorded shader objects, declarations, target formats and specialization inputs. State snapshot precedes mutations; explicit MRT/depth restoration precedes final StateBlock Apply; private resources and INTZ texture owners are released before Reset. Driver completion wait remains startup-only and bounded.

Tests: ordinary and diagnostic x86 builds PASS. Fifteen regression runs PASS: actual warmup REF and HAL/Ex plus native constants, live call results, texture/depth, StateBlock recording/apply, projection history, UP geometry, frame references, buffer version consistency/reentrant callbacks, register banks, replay bindings/surfaces and tentacle head scope. Added persistence assertion: old 1024 parses, 4096 unique records survive serialization, full duplicate differs from capacity rejection. No test creates gameplay shader variants in advance.

Private rollback-protected package: %LOCALAPPDATA%/Darkness2VR/packages/nvidia-warmup-73a461b0-20261030. Contains both DLL/PDB/MAP sets, test logs/hashes, all accepted DLL/INI files plus original warmup/shader caches. Actual pre-test manifest: 1024 records, 3,800,012 bytes. Profile preserves ConsumedRecordCopy=1, DirectRecordCopy=0 and ReplayBufferVersions=0; sets StartupShaderPool=1, PipelineWarmup=1, PipelineWarmupMs=30000. No EXE or asset changes.

Pending ONE consolidated two-launch gameplay check:
1. Learn in the previously affected outdoor combat scene. Preserve log and expanded manifest before closing the game; require new records beyond the original file. Do not delete the old manifest or rerun an OFF baseline.
2. Restart the same candidate, verify warmed>0, driverComplete=1, restored=1, missing/unsupported/timedOut/failed counts, then revisit that combat scene. Existing collector and command history only. Retain exact DLL/PDB/MAP, INI identities and unique app frame intervals. Export history BEFORE closing the process.

Acceptance still NOT RUN: PSVR2 visual result, recurring NVIDIA compiler-stall reduction, gameplay p95/p99 and 90 Hz. The user reports about 20 FPS collector overhead; instrumented frame-time changes must not be represented as uninstrumented FPS. Restore the preserved accepted DLL/INI and caches if crashes, corruption or material regressions appear after retaining evidence.
