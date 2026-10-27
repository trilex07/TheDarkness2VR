# Temporary geometry omission diagnostic

For an in-place comparison, set Render.ObjectDrawPadToggle=1 and
Render.SkipObjectDraws=0. The scene starts visible. L3+R3 queues one toggle
at the next VR-frame entry; release BOTH sticks before clicking again. The
chord temporarily replaces recenter on VR and physical pads; keyboard F8 still
recenters. Input polling never changes D3D state or geometry directly. The log
records `geometry=HIDDEN` / `VISIBLE`. Reset to ObjectDrawPadToggle=0 to restore
the ordinary recenter chord. The visibility flag is not persisted by clicks.

Render.SkipObjectDraws=1 (default 0) omits triangle-list/strip/fan batches with
more than two primitives at all four shared live/replay draw wrappers. This is
an intentionally destructive visual test, not an object classifier: buildings,
cars, characters, weapons, batched HUD or effects can disappear. One/two-triangle
screen quads and lines/points still forward. No engine entities or save data are
deleted. Native culling, animation, simulation, material/state submission and
recorder work remain. Thus this tests draw execution/driver submission, not the
entire CPU cost of scene objects. Occlusion feedback may change later frames.

Load the same save and face the same crowded scene; do not navigate through the
invisible level or save during the test. Report FPS and whether geometry really
disappeared. `[ObjectDrawTest]` counts omitted live/replay draws. If no usable
image remains, close the game; restore the deployment backup or set the flag to
0 and restart. Keep SkipShadowAtlasDraws=0 to isolate this experiment.

Offline x86 regression extracts the actual wrappers and covers default OFF,
native failure forwarding, both replay eyes, small quad preservation, all
triangle types and nontriangle forwarding. Both x86 builds pass. Gameplay
performance and HMD acceptance remain unverified.
