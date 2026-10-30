# Consolidated stability candidate

Base: MAIN 8c33cf84. Integrated recorder-driver-deadlock 9579a958; texture/depth
correction 3067a4bd is already present through fc6012d3. No automatic merge.

Conflicts preserve replay binding filtering and bounded buffer metadata lookup.
Lookup COM references use DriverScope too; Reset refuses busy native/replay
phases before dropping resources, clears metadata pins, and retains ResetEx
replica behavior. The replay packet is immutable during replay; native callbacks
reject ambiguous stereo and journal exact VB/IB ranges for deferred recovery.
Texture/depth successful HRESULT ordering and INTZ ownership remain unchanged.

Validation: both x86 builds PASS. Actual-source VB/IB tests PASS six consistency
cases, eleven bounded callback/worker/write/reset probes, native SEH recovery,
lookup lifetime/collision/Reset cases, quotas and disabled bypass. Texture/depth
1649, live hooks 5688, StateBlock 296171, projection 145248, UP 4403,
hands 2233, left aim 461, tentacles 26629 and native menu 128 checks PASS.
Binding cache, surface receipts, resource lifetime ledger, native camera boundary,
constant float loop (58510 per build flavor), and record-copy 10681 checks PASS.
An integration-only duplicate stateCache declaration in the UP fixture was
removed; the regenerated actual-source fixture passes. No production test
failure was waived.

Polish package source ca3e75ae expands from six to 25 nonempty English atlases:
450 Polish glyphs with original native BC2 pixels and metrics preserved. Thirty
font/package tests PASS; native archive extraction passes on the private stage.
Speech and text files are preserved byte-for-byte. Game assets remain private.

ReplayBufferVersions=0 stays quarantined. Existing CPU/FOV/VR profile is retained.
PSVR2 flicker, stability, Polish glyphs and outdoor CPU frame time require one
consolidated manual session. No runtime FPS, CPU saving or visual fix is claimed.
