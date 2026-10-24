# Rendering clarity and stereo depth diagnosis

Date: 2026-10-24

Base: `origin/research/evolution-engine-render-pipeline` at `cb6638fc1ce429eb0b85177b5b1a33261f1f74d2`

Branch: `research/clarity-depth-calibration`

## Finding

The two symptoms have separate leading explanations. The known DOF/ghost and FXAA bypasses match and were accepted in prior runtime logs, so they are not the leading explanation for softness that remains with those settings enabled. The first verified app-side spatial filtering is the D3D11 eye composite: it center-crops the captured color RT and samples it with a linear sampler into an OpenXR image that can be smaller than the source. Its logged size difference is modest and does not establish that this stage causes the reported degree of blur. For weak depth, the manual `StereoIPD=0.065` is in Evolution Engine units and has no measured conversion to metres; that makes baseline scale the leading unresolved cause. Parallel translation also reduces disparity as `1/Z`, so distant depth naturally becomes more subtle.

No production code was changed. No game was launched, no DLL was deployed, and no HMD validation was performed in this task. The prior runtime captures described below are read-only local artifacts from the existing checkout; they are not committed to this branch.

## Ranked causes

| Symptom | Candidate | Evidence | Confidence |
|---|---|---|---|
| Soft distant detail | D3D11 crop and linear resample to the per-eye OpenXR image | Source RT0 was logged at 3076×3140 and a prior runtime recommended and created 2676×2728 swapchains. `RenderStereoEyeD11` maps the cropped source through `SampleLevel(..., 0)` with `D3D11_FILTER_MIN_MAG_MIP_LINEAR`. This is the first confirmed app-added resample. | High that resampling occurs; medium that it is the main perceived cause, because the resize is modest and there is no same-eye pre/post capture. |
| Soft distant detail | Texture minification and mip choice on specific world surfaces | The setting only raises `D3DSAMP_MAXANISOTROPY`; it does not turn linear/point filters into anisotropic filters. A recorded frame contains both linear and anisotropic minification state changes. Those records are not weighted by texture, draw, or visible distance. | High for the implementation behavior; low-to-medium for contribution to the reported scene. |
| Soft distant detail | Other game post effects, texture mip detail, or the HMD runtime/compositor | The known composite and FXAA paths are bypassed, but other engine passes and final headset optics/runtime processing have not been captured as an A/B pair. | Open. |
| Weak depth after about 1 m | Manual baseline is too small for the game’s world-unit scale | `StereoIPD=0.065` feeds a ±0.0325 game-unit translation. The config and code explicitly call the scale uncalibrated; the normal path does not derive it from OpenXR. The expected disparity changes linearly with the baseline and inversely with depth. | High confidence in the mechanism; medium confidence that it explains this report until the actual world scale is measured in PSVR2. |
| Weak depth after about 1 m | Expected `1/Z` decline in parallel-translation stereo | `StereoShiftMode=0` is parallel translation. It has no finite convergence plane and tends to zero disparity at infinity. This is expected geometry, not a defect by itself. | High. |
| Weak depth after about 1 m | Stereo polarity, double-counted OpenXR eye pose, or submitted FOV mismatch | Code assigns opposite signed shifts to the two eyes, uses a shared head-center projection-layer pose by default, and the base includes the rendered-FOV correction. Prior stationary fusion was reported as working. | Low as the leading cause for the symmetric main camera; current PSVR2 operation is still unverified. |

The correct next step for sharpness is a matched capture through the suspected stages. Do not add generic sharpening. The correct next step for world scale is a metric measurement, not an arbitrary IPD increase.

## Sharpness pipeline evidence

### Game render target and depth

`Darkness2VR.ini` requests `RenderWidth=3076` and `RenderHeight=3140`. These values are a minimum backbuffer request, not a guarantee that every internal deferred buffer uses that size. Prior local runtime logs show a successful D3D9 reset at 3076×3140 and an INTZ depth texture at 3076×3140. The `run_43052_frame_003` trace also records 3076×3140 as the source. This verifies the captured RT0/depth path for that run; it is not a complete inventory of every G-buffer and lighting target.

### Existing post-processing bypasses

`HashBytes` in `src/stereo_replay.inl` is 32-bit FNV-1a over the shader token bytes, including the END token in the count passed by the hook. The current code matches the complete original shader before rewriting it:

| Original pixel-shader FNV-1a | Bypass | Exact-match conditions | Prior runtime evidence |
|---|---|---|---|
| `0x33DE98C1` | Replace the DOF/ghost final blend while retaining the shader layout and other final color work | Full bytecode hash plus one exact blend-token sequence | `[Sharpness] DOF/ghost blend bypass ... result=00000000`; its pixel-shader pointer appears in the saved frame trace. The saved 1,956-byte shader blob independently hashes to `33DE98C1`. |
| `0x75F88E11` | Same DOF/ghost final-blend rewrite | Full bytecode hash plus one exact blend-token sequence | Prior log reports successful creation (`result=00000000`) and records the hash. The pointer was not bound in the selected `run_43052_frame_003` trace, so this specific frame does not prove that variant was drawn. Its saved 1,684-byte shader blob hashes to `75F88E11`. |
| `0x5104FEF7` | Remove FXAA offset edge filtering and keep the center sample | Exact hash and exactly 938 DWORDs | Prior log reports successful creation and the captured trace binds its pixel-shader pointer. |

The runtime log also recorded `DisableDepthOfField=1` and `DisableFXAA=1`. `BuildSharpComposite` and `BuildUnfilteredFxaa` are gated on `SubmitMode=4`, which is the mode used by the cited native-stereo captures. These are verified exact shader matches and successful replacement-shader creation. They do not prove that every possible blur effect is removed or that the replacements improved the final HMD image.

### Texture filtering

`TextureAnisotropy=16` is passed only when the game sets `D3DSAMP_MAXANISOTROPY`. The hook leaves `MINFILTER`, `MAGFILTER`, and `MIPFILTER` unchanged. Direct3D 9 requires an anisotropic minification or magnification filter **and** a maximum anisotropy greater than one; see [Microsoft’s Direct3D 9 anisotropic-filtering documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/anisotropic-texture-filtering).

The captured `run_43052_frame_003_trace.txt` has 450 sampler-state events. For sampler 0 it records 21 linear and 22 anisotropic MINFILTER values; sampler 1 records 16 linear and 16 anisotropic MINFILTER values. It records `MAXANISOTROPY=16` for both samplers. The captured MAGFILTER values are point or linear, not anisotropic. This proves the game selects mixed sampler modes and the configured cap is 16; it does not identify which visible distant materials used each sampler state. Event counts are state changes, not draw or pixel weights.

### App-side capture, crop, and OpenXR images

The native-stereo path captures the active D3D9 RT0 into a shared texture without resizing, or falls back to a readback texture with the RT0 dimensions. It then renders one eye image per OpenXR view. In the native-stereo shader branch, source UV is centered and scaled by the per-eye crop; the color read is `SampleLevel(..., 0)` with a linear D3D11 sampler. The eye cache and swapchain use the same dimensions, and `WriteTextureToSwapchain` uses `CopyResource` into the entire submitted image rectangle. The application therefore adds one crop/resample stage before submission, with no later application-side resize. The runtime and HMD can still filter, distort, or reproject the image.

A previous OpenXR runtime log reported 2676×2728 recommended per-eye images, max 8192×8192, and created swapchains of 2676×2728. Khronos defines these as the runtime’s optimal image-rect dimensions, not as a promise of physical panel pixels ([`XrViewConfigurationView`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrViewConfigurationView.html)).

For that logged source/target pair, with no additional source crop, the output is 87.0% of source width and 86.9% of source height. The current checked-in config separately requests `EdgeCrop=0.08`; if that same setting and those same runtime dimensions are combined, the active source region would be 2848×2907 and the final eye image would be about 94.0% / 93.8% of that region in width / height. This latter figure is a calculation from the config and historical dimensions, not a measurement of one current PSVR2 run.

The available `run_43052_frame_003_eye0.bmp` and `eye1.bmp` captures are 2676×2728. The offline analyzer compared a 512×512 ROI at identical integer coordinates: MAE=0, RMSE=0, maximum difference=0, and equal gradient/Laplacian metrics. This is consistent with the documented zero-shift validation mode. Identical eyes establish equality for that captured pair; they cannot establish that either image is sharp. No same-eye bypass-on/off or RT0-to-eye-cache A/B pair is available, so the first incorrect sharpness stage is not proven.

## Stereo geometry and scale

The production matrix code uses row vectors. In `StereoShiftMode=0`, eye 0 receives `+StereoIPD/2`, eye 1 receives `-StereoIPD/2`, and the clip-space translation is proportional to the projection coefficient. Let:

- `W` be the submitted per-eye image width in pixels,
- `P_eff = P_camera × fovScaleX / cropX = 1 / tan(renderedHalfFovX)`,
- `I_game` be the full inter-eye separation in Evolution Engine world units,
- `Z_game` be camera-to-feature distance in those same units.

Then the expected magnitude of left-right horizontal disparity for parallel translation is:

```text
d_px = W × P_eff × I_game / (2 × Z_game)
```

It decreases as `1/Z_game`; polarity reversal changes its sign. There is no finite zero-parallax plane in this mode; disparity approaches zero at infinity. `StereoShiftMode=1` instead uses a per-eye shear `s=±StereoSep` and convergence distance `C=StereoConv`:

```text
d_px = W × StereoSep × (1 - StereoConv / Z_game)
```

That model has zero disparity at `Z_game=StereoConv`; it changes foreground/background polarity around that plane. The checked-in config selects mode 0, so the shear equation describes the alternate mode only.

The reproducible C++ test calls the same `ApplyEyeShiftToMatrix` and `BuildPerEyeTranslation` helpers as production. With the captured example projection coefficient `P=1.768088`, a 2676-pixel output width, and `I_game=0.065`, it expects 153.771 px at `Z=1` game unit, 76.885 px at `Z=2`, 30.754 px at `Z=5`, and 15.377 px at `Z=10`. These are conditional pixel predictions, not claims that one game unit equals one metre or that the current PSVR2 rendered FOV has `P_eff=1.768088`.

### Why `StereoIPD=0.065` is not a metric calibration

The current INI sets `StereoIPD=0.065`, `StereoShiftMode=0`, and `StereoPolarity=0`. In the code, that is a full separation of 0.065 **game units**. `StereoIpdSource` defaults to manual mode; the optional OpenXR source reads eye-position distance in metres and multiplies it by `GameUnitsPerMeter`, whose code default is 1.0 but is not calibrated for this game. OpenXR physical pose positions are expressed in metres ([`XrPosef`](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrPosef.html)). The normal manual path does not compare those metre positions with the game-unit baseline.

To estimate scale from a known, rigid landmark:

1. Obtain its camera distance `Z_game` from a known in-game measurement or world coordinates, in engine units.
2. Capture the same static feature in both eyes at the same frame and measure `d_px` on the output image grid.
3. Measure `P_eff` from the rendered FOV used for that pair and use the submitted image width `W`.
4. Recover `I_game = 2 × Z_game × |d_px| / (W × P_eff)`.
5. Measure `I_XR_m` from the two `xrLocateViews` positions. If the game world is meant to be metric, estimate `gameUnitsPerMeter = I_game / I_XR_m`; then the metric baseline should be `I_XR_m × gameUnitsPerMeter` game units.

If only a prop size is known, use a prop whose engine-unit dimension and intended real-world dimension are both verified: `gameUnitsPerMeter = propLength_game / propLength_m`. Without at least one verified game-space distance/size and a metric reference, the actual world scale cannot be solved from `StereoIPD` alone.

`StereoLayerPose` defaults to 0, so the two projection views share the head-center layer pose and the eye baseline comes from the game matrix shift. This avoids adding OpenXR’s physical eye separation a second time. `StereoPolarity=0` maps positive shift to eye 0/left and negative shift to eye 1/right; the synthetic geometry test verifies the sign and its reversal.

## Offline tools and results

The branch adds:

- `tools/stereo-math-tests/clarity_depth_test.cpp` and `build-clarity-depth.ps1`: run production matrix helpers at multiple depths, verify polarity reversal, demonstrate the alternate convergence plane, and recover a synthetic units-per-metre scale from a known 2 m feature.
- `tools/clarity_capture_analysis.py`: compare same-sized BMP captures at identical pixel coordinates, compare different-sized captures at matched projection rays against an explicit bilinear crop-resample baseline, measure horizontal disparity for a user-selected ROI, recover scale when the metric inputs are known, and compute the shader FNV-1a fingerprint.

Commands:

```powershell
.\tools\stereo-math-tests\build-clarity-depth.ps1
python .\tools\clarity_capture_analysis.py self-test
python .\tools\clarity_capture_analysis.py compare <same-eye-A.bmp> <same-eye-B.bmp> --roi X Y WIDTH HEIGHT --json
python .\tools\clarity_capture_analysis.py ray-compare <same-frame-RT0.bmp> <same-frame-eye.bmp> --source-fov-tan SL SR SU SD --target-fov-tan TL TR TU TD --crop-x CX --crop-y CY --roi X Y WIDTH HEIGHT --json
python .\tools\clarity_capture_analysis.py measure-disparity <left.bmp> <right.bmp> --roi X Y WIDTH HEIGHT --max-shift 80 --px-effective P --distance-game Z --xr-ipd-m IPD --json
python .\tools\clarity_capture_analysis.py shader-hash <shader-bytecode.bin>
```

`compare` intentionally requires the same dimensions and coordinates; it does not auto-register, crop, or resize the pair. `ray-compare` reads the actual dimensions from both BMPs. Its source and target FOV arguments are tangent bounds in `left, right, up, down` order: source RT0's uncropped frustum and the final per-eye submitted frustum. The target ROI is expressed in target-image pixels. It reports two comparisons: source samples at matching tangent-space rays, and the expected source-to-target D3D11 crop (`uv = 0.5 + (targetUV - 0.5) * crop`) sampled with bilinear/clamp filtering. The latter is the baseline for expected resampling; do not call its detail reduction extra blur. `target_fov_crop_match_max_abs_tangent_error` checks whether the submitted FOV agrees with the RT0 frustum and recorded crop.

For MAIN to run this analysis, capture both images from **one exact game frame**: D3D9 RT0 immediately before the D3D11 eye pass and that pass's complete final eye image. Record the matching frame ID, eye index, exact source and target image/viewport dimensions, source projection matrix after all game and VR FOV edits but before the eye-pass crop, per-eye final submitted `XrFovf` angles, and the exact per-eye `cropX`/`cropY` float values used by the D3D11 shader. Convert the matrix/FOV to the tangent bounds passed to the tool; preserve the raw matrix and radians in the capture record. Also record source/target view poses or view matrices, eye-specific stereo shift/warp inputs, pixel format and color-space state. Those values are needed to identify residuals due to parallax, view changes, tone/color conversion, or other shader work. The analyzer maps rays in a common camera orientation and does not undo 3D translation or depth-dependent stereo reprojection. Use a static ROI whose view and scene are unchanged; interpret nonzero residuals with those limits in mind. Pixel/edge metrics are diagnostics, not proof of perceived sharpness.

Validation performed for this branch:

- C++ test: `PASS: native stereo translation, polarity, shear convergence, and scale recovery`.
- Python image-metric self-test: synthetic blur lowers Laplacian response; identical images return zero difference.
- Python ray-comparison self-test: identity and bilinear baselines match exactly; the centered-crop bilinear baseline matches exactly and ray mapping agrees within one 8-bit level; known post-resample blur is detected against the expected baseline.
- Python disparity self-test: a synthetic five-pixel eye shift is recovered.
- Existing local zero-shift capture ROI: 512×512, MAE=0, RMSE=0, max=0.
- Saved local DOF blobs: FNV-1a `33DE98C1` and `75F88E11` recomputed from 1956-byte and 1684-byte files.
- Runtime shader creation log: bypass creation returned `00000000` for DOF `33DE98C1`, DOF `75F88E11`, and FXAA `5104FEF7`; the saved frame trace binds pointers corresponding to `33DE98C1` and `5104FEF7`.
- No same-frame RT0/final-eye runtime pair is available for the new ray comparison. Further validation requires MAIN to capture one; no headset or in-game test was run in this task.

Local historical evidence paths (ignored/untracked in the original checkout; not part of this branch): `test-output/xr-neutral-43052.log`, `test-output/run_43052_frame_003_trace.txt`, `test-output/run_43052_frame_003_eye0.bmp`, `test-output/run_43052_frame_003_eye1.bmp`, `test-output/ps_inverse_33DE98C1.bin`, and `test-output/ps_inverse_75F88E11.bin`.

## Concrete recommendations for MAIN

1. Keep the exact DOF and FXAA hash guards. They already match; do not widen the match or add another generic sharpening pass.
2. Add an opt-in diagnostic capture of RT0 immediately before the D3D11 eye pass plus the resulting eye cache, logging RT0 size, `P_eff`, crop, OpenXR recommended/max size, and eye-cache size. Compare the source and output at matching normalized rays. The existing `DumpFrames` capture saves final eye images, not the pre-blit source.
3. If that comparison shows meaningful loss in the D3D11 stage, add a diagnostic per-eye render-scale A/B that scales the OpenXR image rect only within `maxImageRectWidth/Height`, leaves projection/FOV/crop fixed, and logs exact dimensions. Raising `RenderWidth/Height` alone is not the test: the logged source is already larger than the recommended eye image.
4. Instrument sampler filter state with the bound material texture and world draw. Only if distant world draws prove to use non-anisotropic minification should MAIN add an opt-in world-material min/mag anisotropy setting. Preserve UI and postprocess sampler states.
5. Calibrate `GameUnitsPerMeter` from a verified in-game measurement and the same-runtime OpenXR eye separation. Add a diagnostic log for `xrLocateViews` inter-eye distance in metres, the configured units-per-metre value, and the resulting game-unit baseline. Then set `StereoIPD = OpenXR_IPD_m × GameUnitsPerMeter` or enable the OpenXR-derived source with that calibrated value. Do not treat the converter’s default 1.0 as measured truth.

## Smallest PSVR2 comparison

Use one stationary loaded level and one clear rigid edge around 1–2 m plus one detailed landmark farther away. Keep the same saved position, headset orientation, OpenXR runtime, FOV, render dimensions, DOF/FXAA bypasses, and lighting between runs. Edit the active INI path printed in the run log and restore it afterward.

1. Capture a control pair with `[Render] StereoMode=2` (the documented no-shift validation mode). Set `DumpFrames=2` and `DumpDelay=120`; after at least 120 native-stereo frames in the loaded scene, the game writes two eye BMPs and a trace under `test-output`.
2. Restart with only `StereoMode=1`, capture the same view again, and return to the PSVR2. Close one eye at a time to judge image detail independently of binocular depth; then open both eyes and compare whether the near and far landmark separate in depth.
3. Run `measure-disparity` on the mode-1 left/right pair with a distinctive static ROI. Supply `--distance-game`, `--px-effective`, and `--xr-ipd-m` only when each value has a measured source. Use `compare` only for same-eye captures with unchanged geometry/projection (for example, an A/B filter comparison); mode 2 and mode 1 intentionally move features, so their raw same-index pixel difference is not a sharpness measure.

Interpretation: if each monocular view looks equally soft in both modes while binocular depth changes, sharpness and geometry are separate. If the no-shift control is already soft, the baseline is not the sharpness cause. If a known-distance feature has much less disparity than the calibrated formula predicts, update the game-unit scale before changing convergence or polarity. This comparison can classify the next step; it cannot certify overall VR sharpness or world scale by itself.
