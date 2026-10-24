# MAIN integration and matched-frame diagnostic — 2026-10-24

## Integration receipt

Clean managed worktree: `integration-frame-capture/Darkness2VR`.
Branch: `codex/integration-frame-capture`, from fetched
`origin/research/evolution-engine-render-pipeline` at `cb6638f`.
Cherry-picks, in the requested order:

- Luna `744e637` → `eeef294`; `accd9b2` → `2965179`.
- Sol `f978db8` → `bd196c8`; `5f59fe1` → `b7f57dd`.
- Astra `ddaf4af` → `7bbc4fd`.

No Gemini adapter, merge, deployment, Steam verification or game launch.
The five integrated commits change no production `src/`, `build.ps1` or
`Darkness2VR.ini` relative to the research base. The original checkout and its
untracked MODLOG were not edited. Original MODLOG SHA-256:
`74B7C6D350609FE30A3AA745B1EFF533D83F9E34DE993E4E668C8CBF9A773002`.

## Implemented experiment

`build.ps1 -DiagnosticCapture` defines `D2VR_MATCHED_CAPTURE`. An ordinary build
does not compile the capture code, its draw observers, GPU reads or hashing.
The diagnostic build is also OFF at runtime until explicitly requested:

```ini
; Darkness2VR-diagnostic.ini beside the game, after the runtime gate is cleared
[Diagnostics]
MatchedFrameCapture=1
MatchedCaptureSerial=1
```

Increasing the positive serial requests one next render-heuristic-loaded frame.
It is consumed once, including on failure; there is no automatic retry. Arming
requires SubmitMode=4, StereoMode=1, StereoDebugMode=0 and an observed Present
boundary. The existing frame schedule is unchanged. No menu is intentionally
captured; completion additionally requires a current world-camera witness.
The renderer's gameplay/world heuristic remains a heuristic, not a proven
native gameplay-state API.

Files go to a newly created `%TEMP%/Darkness2VR-matched-PID-SERIAL-XRFRAME/`:

- `source-eye0.bmp`: active D3D9 RT0 before any right-eye replay.
- `source-eye1.bmp`: RT0 after that frame's replay, before restore/upload.
- `final-eye0.bmp`, `final-eye1.bmp`: final D3D11 caches before swapchain copy.
- `manifest.jsonl`: per-stage frame/eye identities, source and upload RT identity,
  dimensions, formats, source/draw/blit viewports, shader identities and detected
  WVP/STP register bases, per-eye scaling, crop and head matrices, fresh native
  inverse-projection **candidate** and Px/Py, actual 24-float D3D11 blit buffer,
  sRGB write/sampler flags, D3D11 SRV/RTV formats and filtering, final submitted
  XrFovf and layer/located-eye poses, XR eye separation, game-unit baseline,
  build/EXE/INI/request hashes, pair frame and proxy Present counter.
- `vs-banks.bin`: one 4096-byte little-endian float32 bank per draw, in manifest
  order. Register `cN` begins at `bank_offset + 16*N`. These are API-visible
  constants immediately before the draw, after replay/matrix edits, including
  state restored through stateblocks. Fixed-function world/view/projection
  matrices are separately recorded with validity. Register-base recognition
  alone does not establish active-world-camera ownership.
- `effective-config.json`: effective stereo/camera/lighting switches and latched
  per-eye parameters. The actual blit constants and complete draw banks are the
  corresponding rendering-state witnesses; the original selected INI is hashed.
- `COMPLETE.json`: SHA-256 receipt for every required file, written only after
  successful same-frame stereo submission and successful file closure.

Unsupported MSAA/source formats, missing current projection/world witnesses,
fallback to backbuffer, different production input, duplicate stages, missing
eyes/draws, setting changes, frame rollover, file/readback failure and failed
submission cannot produce a complete receipt. Accepted source formats are
A8R8G8B8/X8R8G8B8; final formats are RGBA/BGRA UNORM or sRGB. BMP output is
lossless 32-bit BGRA with RGBA swizzled explicitly; alpha is not used by Luna.
Each image is limited to 16,777,216 pixels, each eye to 16,384 draw banks.
GPU readbacks deliberately stall the requested frame: this is not an FPS test.

Engine simulation-frame ID and active camera identity are still unproven and
are not invented: `engine_frame_id` is null. Matching is by the existing
record/replay XR-frame context and observed proxy Present boundaries.

## Native-patch isolation and presentation gate

The diagnostic build suppresses native gameplay installers, FOV installation
(including the later TryApplyEngineFovOverride path), native base-FOV setter,
EXE Present-dispatch patch, XInput IAT changes and GetClientRect IAT changes.
D3D9 proxy/COM interception and SubmitMode=4 remain available. Production
builds retain all their existing paths.

This exposes an explicit unresolved gate: if the game reaches Present only
through the suppressed EXE dispatch, existing COM callbacks may not observe it.
The capture refuses an unobserved Present boundary instead of accepting an
EndScene fallback as a matched game frame. No new boundary adapter was added.
Real capture capability in this native-patch-free mode therefore remains
NOT RUN, and may require a separately proven COM presentation observation.
Likewise, disabling the resolution IAT hook may change diagnostic target sizing;
the manifest records actual sizes. Do not present this variant as identical
runtime behavior to the user's shipping mod.

## Offline analysis and verification

```powershell
python -m unittest discover -s tools -p test_matched_capture.py -v
python -m unittest discover -s tools/native-camera -p test_*.py -v
python tools/clarity_capture_analysis.py self-test
./tools/stereo-math-tests/build-clarity-depth.ps1
./tools/stereo-math-tests/build-rendered-fov.ps1
./test/build_test.ps1
./build.ps1 -DiagnosticCapture
./build.ps1
python tools/check_matched_capture.py <capture-folder>
```

All ten matched-capture tests and six PE/audit parser tests pass. The synthetic
matched fixture invokes Luna's actual ray-compare CLI successfully. Luna's
self-test passes, clarity/depth C++ tests pass, 1206 rendered-FOV rays pass, and
the existing real-D3D9 synthetic warp harness reports four PASS cases/exit 0.
Both x86 DLL variants build. This does not exercise the new readback code on a
live game or establish a blur cause.

For ray comparison, supply `--eye`, independently established
`--source-fov-tan L R U D`, and `--camera-proof-note` identifying the verified
world draw, register range and matrix convention. The checker validates file
hashes, same-frame stage ordering, image dimensions, draw banks and completeness
before invoking Luna with the actual crop and submitted tangent bounds.
It refuses unsupported viewport/pose/blit assumptions, including head-geometry
cases without a separately established common pose. The native inverse matrix
candidate is not automatically promoted to the source camera. Raw Luna remains
available for a separately reviewed richer transform analysis.

An ordinary build and a clean integrated-base build have identical preprocessed
source after whitespace normalization (MSVC /EP, same SDK): SHA-256
`a9bcfd7a4e2f507716bc777e0f5918c16d4155728b039ea5583e8b4dcfa36cd9`.
No capture statements enter production compilation. Raw DLL/.text hashes from
different debug-build directories differed; they are not claimed identical.
The production INI is unchanged. No runtime performance claim is inferred from
the synthetic tests.

## Restored installation: identity passes, Steam gate still closed

The installed EXE independently reappeared; MAIN did not restore it. A fresh
read-only audit reports original SHA-256
`55c8d634fcbe2996d8397d15d68ea728c999697f1dd40e8779b5afe8bbb4c339`,
PE32/I386, timestamp `4F68A875`, base `00400000`, image size `00DEC000`.
All 14 instruction sites and 10 complete instruction ranges match. Unknown
hashes remain rejected, even if instruction sites match.
Installed INI SHA-256 observed read-only:
`C802439A7AD84E6B4CEA567405B3A5B85BEEA89EB0A00DBF7313FA5437E77436`.

The user has not yet confirmed resolution/verification of Bitdefender's
detection. Identity success alone does not clear that condition. Steam and
PSVR2 tests are NOT RUN; no deployment was made. After that external condition
is resolved, re-audit the installation again, back up deployment files, verify
the candidate hash, and first prove native-patch-free Present observation before
requesting one stationary physical test. No matched game capture exists yet;
sharpness, native-wide FOV and visibility success remain unclaimed.
