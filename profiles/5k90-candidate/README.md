# Combined 5K / 90 Hz candidate

Opt-in diagnostic profile, derived from the physically tested 6008c1e profile.
Only RenderWidth/RenderHeight change: 3076x3140 -> 2560x2614 per eye.
Combined width is 5120; pair pixel count falls 30.7%, from 19,317,280 to 13,383,680.
This is a workload reduction, NOT evidence of sustained 90 FPS. The previous
12-17 ms D3D9 interval exceeded the 11.11 ms total frame budget; D3D11/XR cost
also matters. Measure the same loaded scene; 45 FPS reprojection is not 90 FPS.

Use the diagnostic DLL built with -DiagnosticCapture. Capture remains OFF.
The explicit EnginePresentCallsite, NativeCameraCover and tracked-gameplay
opt-ins retain their existing executable identity/instruction gates and modify
instructions only in process memory. Do not use this profile without the
established MAIN backup/audit/restore procedure. No EXE changes on disk.
Default repository configuration and Steam installation are not modified.

The source change refreshes both frozen eye transforms from the selected native
camera every frame, even when Px/Py stay constant. Shader probes may have
replaced the shared inverse projection in between; clip planes may also change
without FOV changing. Capture rejects headers describing different transforms.
No HMD smoothing, stereo separation change, new FOV formula or shader change.

Offline validation: diagnostic + production builds PASS; 48 camera-refresh
state-contract checks (actual helper, instrumented builder double); 7126 camera
cover checks including x86 shim; 57 XInput/null-model checks; 36 Python Present,
matched-capture and transfer-order tests PASS. Physical 90 FPS acceptance pending.
Frozen human arms, throw-direction consistency, and tentacle head following
remain unresolved; this candidate does not claim fixes for them.
