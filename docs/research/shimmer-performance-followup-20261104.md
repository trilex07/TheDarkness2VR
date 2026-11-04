# Follow-up after user rejects SMAA effectiveness

User says scene/input candidate works; continue FPS and edge shimmer work.
SMAA after NIS gives no perceived headset benefit. Do not call shimmer solved.

## Direct evidence from latest user run

Private snapshot: test-output/shimmer-followup-20261104/user-test.log and
metrics.json. This is the physical SteamVR run, not the simulator fixture.
Recommended eye/swapchain/native eye source all2676x2728; NIS mode2 logs
2676x2728->2676x2728. The earlier3232x3300 simulator test is separate.
There is no NIS spatial upscaling in THIS recorded route. Native intermediate
passes may use other dimensions; final-eye dimensions do not certify those.

The log alternates tracked gaze and no gaze. Current production without the
follow-up patch uses copy-base+two gaze rectangles while tracking. With gaze
missing, it dispatches uniform NisSharpness0.80 over the entire image.
With tracking, effective untouched periphery is exact copied input, transition
uses0.68, center1.00. Thus tracking loss changes pixels outside the gaze region.
This is a proven policy discontinuity, not proof of the entire shimmer cause.

48 cadence aggregates range78..90 layers/s. Mixed menu/world windows can show
world-bearing32.8 with83.2 boundaries/s; do not label that as32.8 gameplay FPS.
GPU timestamp split170 samples: live0.264..6.436ms, replay3.198..9.997ms.
These spans include submission gaps and are NOT GPU Busy. CPU segment171 means:
left0.05..0.94, replay/history0.05..1.76, right/restore0.39..2.13ms. These scopes
exclude much of native game execution and are not aligned with the GPU windows.
Worker mean preparation0.0925..0.9567ms, join0.0001..0.0004ms. No basis for moving
another arbitrary tiny computation to a worker. No25ms/40FPS recovery proven.
Boundary-p95 always16ms has coarse timer limits; not a useful hitch histogram.

## Concrete correction prepared

Render/FoveaCenterFallback default1: a missing gaze uses fixed image center for
that eye. NIS remains local with copy-base; old uniform fallback available at0.
No FXAA, new temporal history, changed resolution, changed FOV or fake vectors.
Actual-source test uses a textured eye, constant independent opposite eye,
tracked center, both-eye loss, either-eye loss and recovery. Exact pixels must
match the centered tracked reference; all pixels outside the region equal input.
Legacy0 must still reproduce uniform80% reference. Before patch FAIL at first
lost-gaze comparison. After patch PASS WARP and RTX5070. Both x86 builds PASS.
Private logs: build/gaze-blink-before.log, gaze-blink-after-{warp,hardware}.log,
gaze-blink-{production,diagnostic}.log. These are isolated actual-code GPU tests,
not a new in-game/HMD acceptance or a proven cure for geometric edge shimmer.

## External reports and primary implementation guidance

- Resident Evil mod users report shimmering hair/white outlines despite AA:
  https://www.reddit.com/r/virtualreality/comments/1aeet4p/
  Their anecdotes do not establish our cause. Official REFramework docs instead
  describe engine-specific TAA fixes/new renderer in pd-upscaler builds:
  https://refdocs.praydog.com/troubleshooting/VR-Troubleshooting.html
  Lesson: engine history/stereo integration matters; importing its settings into
  Evolution/D3D9 is not a working temporal AA implementation.
- Skyrim VR reports associate similar outlines with shaders/sharpening/foveation:
  https://www.reddit.com/r/skyrimvr/comments/1ccqk18/
  Conflicting individual outcomes; treat as test leads, not universal fixes.
- vrperfkit configuration documents optional mip bias and potential artifacts:
  https://github.com/fholger/vrperfkit/blob/master/vrperfkit.yml
  Audit native texture mip chain and sampler state on exact offending materials
  before any global LOD bias; our hand shader is not proven to share diffuse
  sampler correction's exact VS/PS pair.
- NVIDIA describes NVSharpen as sharpening-only, without upscaling:
  https://github.com/NVIDIAGameWorks/NVIDIAImageScaling
  Do not blame driver-level NIS resolution scaling for our embedded mode2 path.
- Original DarknessII users also report stutter, including on high-end Win11:
  https://steamcommunity.com/app/67370/discussions/0/682989526499598541/
  No controlled fix is demonstrated there. Hyperthread/affinity advice is not
  adopted: our native worker and historical scheduler evidence contradicts
  treating the game as simply using only one CPU core.

## Next discriminating work

1. Remove rejected SMAA cost while preserving user-accepted scene/input fixes.
2. Compare fixed-center fallback under actual tracking loss without changing NIS
   strength or resolution. This separates sharpen policy flicker from aliasing.
3. Inspect actual pre-NIS/native hand and outline pixels with mip/sampler and
   depth/material identities. Geometry contours, specular shading and lost meshes
   require different corrections. Do not add another full-image blur filter.
4. Correlate current long combat frames across native CPU, D3D9 replay/queue,
   transfer waits and XR delivery. Current aggregate spans cannot decide CPU
   execution versus GPU wait or compositor pacing. Retained older ETL identifies
   historical waits but cannot certify current savings. No resolution reduction.
