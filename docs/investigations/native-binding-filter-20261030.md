# Frame-owned native binding candidate (initial offline record)

Historical evidence at 1c70e039. Current consolidation/deployment: [native CPU candidate](native-cpu-consolidated-20261030.md).

Accepted Steam baseline remains 4a26aabd, ConsumedRecordCopy=1; no deployment or gameplay run performed.

NativeBindingFilter defaults OFF and requires NativeSubmissionFilter=1. It elides an exact repeated native VS/PS, declaration, index, stream (identity/offset/stride) or frequency binding and its redundant recorder command only after D3D_OK plus exact successful recorder publication. The existing frame ledger retains non-null COM identities; no additional AddRef/Release or GPU synchronization is introduced. Shader metadata refresh remains active even when a native shader bind is elided. Textures/depth/RT are excluded.

Admission reuses the fail-closed device/thread/opaque-state ownership gate. Ledger release invalidates BEFORE Release callbacks. UP and FVF invalidate both sides; Present, Reset and raw replay use the existing epoch boundaries. Missing recording, overflow, S_FALSE/failure, reentry and unowned publication cannot certify a cache entry. Pointer equality never crosses a frame lifetime.

Retained capture 20261030T205050Z-eab0975b, combat TID39476, 1586 app intervals: exact matching six binding API/hook symbols account for 756 samples (0.47667 ms/frame); float setter subset from the previous candidate is 1.41362 ms/frame. These subsets are exposure, include necessary work and are not measured savings. No new capture or profiler.

Validation: both x86 builds PASS. Twenty affected suites PASS, including actual source shader/geometry ON paths, real COM ledger lifetime/application Release and frame identity reuse, UP/FVF/recording invalidation, six buffer consistency cases and eleven bounded reentrant callbacks. CPU snapshot fixture compiled but execution returned Access denied (environment BLOCKED); no snapshot PASS claimed for this run. Hardware/PSVR2 and runtime CPU/FPS remain NOT RUN.

A separate offline package native-binding-filter-20261030 preserves ordinary/diagnostic DLL/PDB/MAP. This candidate is kept separate from the subsequent NVIDIA warmup integration and is not deployed.
