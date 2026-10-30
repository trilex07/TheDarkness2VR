# Outdoor combat critical-path handoff — 2026-10-30

One authorized capture: 5 seconds stationary, 25 seconds combat. No second
capture, experimental renderer changes, deployment, or game launch was made.
This is evidence for implementation, not a successful 90 Hz correction.

## Identity and retained inputs

Capture: `%LOCALAPPDATA%\Darkness2VR\perf\20261030T141042Z-3536db8b`.
The adjacent `prepared-21e-combat30-20261030` preserves the DLL and four INIs.
Accepted source: `21e9b5b5`; integrated branch before analysis: `c9c5c1f1`.
Installed DLL SHA-256:
`CA31CA7563FB6EBBD6E3C1F41858F61DB34B5187A3129FA6C9A645299CA19941`.
Before/after DLL and INI hashes match. DirectRecordCopy=0 and
ReplayBufferVersions=0 remain quarantined; current accepted optimizations remain.
Matching EXE/INI hashes, phase markers and process identities are in manifest.json.
Raw ETL, PresentMon CSV, PerfClock logs and reducers remain private at that path;
`combat-20261030-evidence.json` fingerprints the retained analysis inputs.

The original PDB was overwritten: installed RSDS age20, available PDB age32.
A rebuild is NOT a matching capture PDB and was not deployed. Attribution below
uses OS symbols, raw addresses and byte-identical function ranges verified in
installed DLL versus a separate accepted-source rebuild. Complete native caller
stacks are not available for every WOW64 sample; that acceptance item is unmet.

## Observed frames and measurement limits

PID34992, render TID2364, NVIDIA worker36588, transfer worker22732.
QPC frequency10000000; ETL origin251539211027. ETW and PresentMon event loss0.
Combat contains1271 complete single-swapchain application intervals: mean18.34353
ms, p9522.74545 ms, maximum44.5716 ms. GPU Busy mean5.96591 ms,
p956.78515 ms, maximum7.424 ms. These are not headset refresh/FPS measurements.
PresentMon CPUWait/CPUBusy are not scheduler RUN/READY/WAIT partitions.

Covered QPC251621686026..251854832288. The remaining1684.6732 ms of the
combat phase contains no completed Present:1666.9703 ms off CPU, predominantly
native SleepEx wrapper0x998130 (return0x99813D). Do not count that tail as a
1.68-second rendered frame, infer missing events, or remove the native sleep.
The reason the native idle path was entered is not proven.

Profiling is intrusive:1036 render samples include ETW stack/context-switch
collection, approximately0.815 ms per observed frame at nominal1ms sampling.
Indirect overhead is unknown; do not subtract this and claim unprofiled FPS.
Sample Count is a sample count, not microseconds. Worker time overlaps render
thread time and is not additive critical-path exposure.

## Ranked implementation targets

1. **Recurring recorder copy:** RecPushLocked -> ReadProcessMemory.
   Proxy return57A366CE, loaded base579F0000, RVA466CE; installed call
   RVA466C8 targets the ReadProcessMemory IAT entry.1513 inclusive samples,
   approximately1.1904 ms/observed frame; all RPM stacks2168/~1.7057 ms/frame.
   RecPushLocked RVA465A0..46790 has identical496 bytes in the source rebuild
   and installed image (SHA072D579A7B4DEAAA6BF7AD530C49E8961E13691386D6884CFBFCCD7C5021381D).
   Recoverable time: unknown, bounded by measured exposure, not all1.19 ms.
   Existing ownership-proven copies already run; remaining source-owner chains
   are truncated. Post-native GetVS/PSShaderConstantF is not a correct substitute:
   native setters may upload eye-adjusted constants while replay needs originals.
   No blanket direct copy, permission cache, or removal of ownership checks.

2. **Driver compiler-linked hitches, not steady frame cost.**
   QPC251783666107..251784111823:44.5716 ms frame, render RUN19.7436,
   off24.828, READY0.312 ms (READY is part of off).
   Native478C30 ->7ABE70 ->BC9A40, returnBC9B5F -> D3D9 CreateTexture ->
   NVIDIA -> WaitForSingleObject. Two waits2.705+17.044=19.749 ms.
   Releasing worker36588 executes nvgpucomp32 in18/20 sampled leaves during
   those waits. The observed API boundary is not proof of texture allocation cost.
   QPC251783258121..251783666107:40.7986 ms, RUN19.7586, off21.040,
   READY0.302 ms. Producer GetData FLUSH waits17.950 ms;15/18 worker samples
   execute nvgpucomp32. Entire combat has only85 compiler-inclusive samples:
   these hitches do not explain persistent18–25 ms frames.
   Recoverable: unknown;19.749/17.950 ms are hitch exposure, not promised savings.
   Cache and offline compiler policy are already ON (read-only NVAPI audit).
   Shader object prewarming is active; the queued driver pipeline variant is
   unidentified. Texture identity/lifetime inspection found distinct native owners,
   no valid cache/prewarm boundary. Do not reuse resources speculatively.

3. **Other blocking boundary:**29.7745 ms frame has9.869 ms Steam wait.
   Native8419C0, return8419E2 ->4F1BF0, return4F1DE5 ->steamclient ->
   tier0_s ->WaitForSingleObject. RUN17.3025, off12.472, READY0.157 ms.
   Exact public API/necessity is unidentified. Recoverable time unknown;
   do not suppress Steam polling/callbacks based on this stack.

The transfer boundary above is VrFrame ->RenderEyePairFromReplay ->
StartStereoTransfer ->CaptureBackbufferToShared ->producerQuery.GetData
(nullptr,0,D3DGETDATA_FLUSH), proxy return57A045A8/RVA145A8. Owner issues
one FLUSH before worker non-flushing polling. Removing it can prevent progress;
moving it can move blocking into the multithreaded D3D9 driver lock. Neither is
an evidence-backed optimization. Last-reader and producer fences remain intact.

## Exact handoff and outcome

Existing SIDE native-producer chat01a18978-1fee-7463-b6e5-580b4dbb1a3a
received addresses, intervals and RPM target; its native ownership review found
no safe texture caching/prewarming correction. Existing SIDE replay chat
01a18c16-3aae-7032-8a20-e1c26885f6b1 reviewed the producer fence and found
no safe removal/move. No new agents, capture, broad profiler or EXE patch.

Implemented renderer fix from this capture: NONE. No defensible whole-ms saving
has yet been established; accepted DLL/configuration stay untouched. The next
code decision needs a proved remaining RPM source contract or an identified
reusable driver pipeline, not another reconstruction of the closed native
resource cycle. Stable native90 FPS remains unachieved. Offline tests of prior
candidates are not runtime safety or improvement evidence for a new change.
