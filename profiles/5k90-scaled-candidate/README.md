# Opt-in 5K output / NIS scaling / tentacle head pose candidate

This profile renders the D3D9 scene at 2048x2091 per eye, then keeps the
OpenXR swapchain at the runtime's recommended resolution (2676x2728 per eye
on the tested PSVR2). `NisMode=3` filters each completed D3D11 eye cache,
including its existing aim marker, into that swapchain size. The two source
images remain geometrically distinct and use the existing SubmitMode=4 path.
If NIS initialization fails, the full-size eye blit is used; a later scaling
failure drops one world layer and rebuilds the full-size cache next frame.
The previous native-size NIS profile and ordinary default remain unchanged.

`HeadAttachedTentacles=1` now identifies WVP c0 for the three exact captured
player tentacle shaders, whose CTAB nests it under `Instance`. Their manually
indexed c45 bone instructions do not set the old `hasBones` flag. The head
attachment is limited further by the player tentacle WVP c3 signature observed
in four matched gameplay captures (w about zero and z about -0.1); the shared
shader bytecodes also draw hand and world meshes. Other WVPs retain their
prior handling. A bounded runtime log records actual head-attachment hits.

Offline: diagnostic and production x86 builds PASS, 25 matched-capture tests
PASS, seven executable-audit tests PASS, D3D9 stereo geometry PASS, and the
D3D11 WARP NIS test passes both sharpen and 256-to-320 scaling in both eyes.
Steam and PSVR2 for this candidate: NOT RUN. 90 FPS, visual quality and hand
tracking are not accepted. The prior trial's DLL and INIs were restored with
their original hashes. Frozen human hands need a separate correction.
