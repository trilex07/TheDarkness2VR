# Opt-in NIS / player-tentacle experiment

Based on the previously tested 5K profile. Native D3D9 source remains
2560x2614 per eye; the OpenXR eye cache is 2676x2728 on the tested PSVR2.
NisMode=2 filters each completed D3D11 eye with NVIDIA Image Scaling v1.0.3
sharpen-only at native cache size, with NisSharpness=0.20. It does not reduce
render cost or guarantee 90 FPS. The original, untouched 5K profile remains
available for A/B. NIS is OFF by default in ordinary installs. Embedded
DXBC bytecode and its MIT source/license are included; shader creation failure
falls back to the original eye textures.

HeadAttachedTentacles=1 omits the world-view head delta for the three bytecode-
identified player tentacle vertex shaders (head/teeth/tail). It retains the
per-eye FOV and separation. This is a reversible trial: shared shader bytecode
could be used by other skinned meshes, and PSVR2 validation is required before
accepting the behavior. The switch is OFF by default in ordinary installs.

D3D9 shadow-atlas classification now caches a successful RT/viewport query until
either state changes; live hooks, state blocks, replay operations and restores
invalidate it. This removes redundant driver reads from frequent VS constant
uploads without changing the atlas predicate.

Frozen human hands remain unresolved. CPU palette writes were observed, but the
visible skin did not move in the last PSVR2 test. Do not claim this experiment
fixes the hand model. Performance at 5K also remains below 90 FPS; NIS may add
GPU time. Test these changes separately from future CPU/GPU scheduling fixes.


Offline verification: diagnostic and production x86 builds passed. The copied
vendor source/bytecode hashes match SingularityVR. A real D3D11 WARP dispatch
with separate eye sources returned identical eye pixels for identical input and
changed 3,408 components of a softened-edge fixture. Native camera, stereo
transfer and NIS bytecode checks passed. Steam and PSVR2 for this candidate
have not run; performance and shader ownership remain live-test questions.

The native hand renderer was inspected further: 0x79927D sets the per-skin
invalid byte, while 0x6C3C71 reads that byte. The current hand edit occurs in
the pistol comparison callback; this disassembly alone does not prove that the
visible FP skin consumes the edited human bones in time. No new native hook is
installed from these addresses. Previous direct palette patch caused an intro
crash, so this candidate keeps the last accepted hand code unchanged.
