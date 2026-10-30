# PipelineWarmup consolidated runtime validation (2026-10-30)

Deployed source: 020d6799ab094023235b9f07bdfdfcc8fabb2119. DLL SHA-256: 450F4CC25BC185491E071E65A98F41EF06F02FCA6DD2A65DDFD3E07B5E6249A0.

Retained capture: 20261030T174801Z-63aeb99c, complete with zero ETW and PresentMon event loss. Matching symbols, effective INIs and original command history remain in the private capture/package. ETL SHA-256: 50C7E9B472CED6233987119590506AA56D64CBABD4C38F5B437667AD106A1700. History SHA-256: 3EEAF7232D912FF76DB32322996B9E7EB3F7240D474B4A10945864DF0FED6B27.

Startup: records=621 warmed=621 draws=2325 unsupported=0 missingObjects=0 driverComplete=1 restored=1 elapsedMs=453. The INTZ ownership correction is functionally validated in game. This does not establish exhaustive driver pipeline coverage or hitch elimination.

A compiler dependency remains: history sequence 1186181, frame 19769, render TID 11016, QPC 382108766370..382109675557 at 10 MHz. VrFrame -> RenderEyePairFromReplay -> StartStereoTransfer -> CaptureBackbufferToShared -> producerQuery.GetData(D3DGETDATA_FLUSH) lasted 90.9187 ms. The render thread waited 90.878 ms (UserRequest); NVIDIA worker 35924 readied it through ReleaseSemaphore. 88/91 worker samples in this interval contain nvgpucomp32.dll. Exact shader/variant attribution is not established. Do not compare this event as a controlled regression against the different earlier 21 ms event.

Instrumented combat: 1418 unique application intervals; mean 17.6341 ms (56.7082 FPS), p95 19.5812 ms, p99 21.1135 ms, worst 110.5753 ms. GPU Busy mean 6.0025 ms, p95 6.6804 ms. User reported the collector reduced performance by about 20 FPS. These are instrumented results, not ordinary gameplay performance; do not subtract an estimated overhead to invent corrected FPS. Comprehensive headset visual acceptance remains pending.

Preserve warmup ON and the verified rollback; no more baseline run. Next sustained-CPU correction targets RecPushLocked -> ReadProcessMemory using independently gated consumed constant payloads. Current capture exposes approximately 1 ms/frame of that path, with instrumentation and incomplete unwind limitations. This is exposure, not a promised saving.
