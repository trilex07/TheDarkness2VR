# Opt-in 5K output / NIS scaling / tentacle head pose candidate

This profile renders the D3D9 scene at 2048x2091 per eye, then keeps the
OpenXR swapchain at the runtime's recommended resolution (2676x2728 per eye
on the tested PSVR2). `NisMode=3` filters each completed D3D11 eye cache,
including its existing aim marker, into that swapchain size. The two source
images remain geometrically distinct and use the existing SubmitMode=4 path.
If NIS initialization fails, the full-size eye blit is used; a later scaling
failure drops one world layer and rebuilds the full-size cache next frame.
The previous native-size NIS profile and ordinary default remain unchanged.

`HeadAttachedTentacles=1` identifies WVP c0 for three captured skinned shaders,
whose CTAB nests it under `Instance`. These shaders also draw other meshes.
Every upload gets the ordinary world head transform. Only at an indexed draw
does the mod cancel that transform, after checking the bound VB/IB sizes,
stride, exact stream offset and player WVP c3 signature. The captured frame
selects all 12 player tentacle draws and rejects 13 other draws sharing those
shaders. Unknown meshes retain the ordinary world pose. A bounded runtime log
records actual mesh-scoped hits.

The 2026-10-25 headset run of 9dfb2c1 showed that both dirty bytes did not
make the human hands follow the controllers. It also exposed a world-pose
regression: non-tentacle draws using the same skinned shader skipped their
normal head transform. The current candidate removes that skip and applies the
tentacle exception at the identified mesh draw only. `NativeHandsDirtyBoth=0`
rejects the unsuccessful hand experiment. Hand IK now resolves the avatar model
through its previously verified +1F8C reference, checks its renderer and
130-bone skeleton, then writes there; the old path followed the pistol model's
parent. This source-level owner correction still needs a physical retest.

`GpuSplitTiming=1` adds one optional D3D9 timestamp between the live render
and right-eye replay. The prior headset trial measured around 49-53 world
layers/s and 15 ms D3D9 GPU time in the heavy scene, excluding D3D11/OpenXR.
The split will identify whether the live or replay half dominates on the next
controlled run. It does not change rendering or enable a performance shortcut.

Offline: diagnostic and production x86 builds PASS, 25 matched-capture tests
PASS, seven executable-audit tests PASS, D3D9 stereo geometry PASS, and the
D3D11 WARP NIS test passes both sharpen and 256-to-320 scaling in both eyes.
Steam and PSVR2 for the corrected candidate: NOT RUN. The previous 9dfb2c1
trial demonstrated actual NIS output, slight physical sharpness improvement,
frozen hands, incorrect world/head behavior, a displaced pistol, and FPS below
90. The game exited and its DLL and INIs were restored with original hashes.
The avatar skin owner is selected by validated object links, but visible hand
motion is unconfirmed until the next headset run. If it remains frozen, compare
this renderer to the D3D9 hand draw and its uploaded c45 palette before any
further palette or native hook change.
