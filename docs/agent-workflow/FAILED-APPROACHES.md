# Failed Approaches Index

### Shrine asset names and partial draw deltas did not identify the symbols
- **Date**: 2026-11-05
- **Scope**: MAIN, user-confirmed purple angular shrine symbols.
- **Failure**: GlyphMat color suppression (base/DLC) and degenerate
  TalentGlyphARaised/Sphere meshes left the target. Replacing RaisedMat's shader
  produced black spikes. All reverted. A selected BlackHoleEye draw with zero
  delta was also insufficient negative evidence from an incomplete capture.
- **Resolution**: base TalentShrineBlackHoleMat EyeColor/HiLiteColor suppression
  visibly removed the confirmed symbols, retaining other shrine effects.
  See handoffs/HANDOFF-shrine-icon-20261105.md for evidence and candidate.

### TalentBreadcrumb name alone does not identify the moving shrine icon
- **Date**: 2026-11-05
- **Scope**: MAIN shrine-icon simulator investigation, no production patch.
- **Failure**: actual HUD.swf character23 TalentBreadcrumb contains an animated
  Text child. Hiding this clip was not demonstrated to remove the user's
  head-following purple shrine overlay. Trial was rolled back and discarded.
- **Next**: identify the exact visible element; inspect ContextActionPane or
  shrine prefab producers only with matching runtime evidence. Preserve the
  world flame and interaction. See handoffs/HANDOFF-shrine-icon-20261105.md.

### Native skin derivative variance estimate did not meet shimmer quality gate
- **Date**: 2026-11-04
- **Scope**: MAIN BC938BC9 exact native PS; default-off private experiment.
- **Failure**: derivative normal-variance/gloss reduction passed flat/diffuse
  preservation but normalized temporal variation only0.622177 ->0.553146
  (about11%), failing the preselected20% reduction gate on actual D3D9Ex GPU.
  Did not lower that gate or deploy this algorithm. Native XY-to-unit-Z decode
  also discards the normal-length data needed by a direct Toksvig estimate.
- **Next**: four spatial normal samples per Phong lobe passed the same fixture;
  still not mesh-edge AA or headset acceptance. See
  `docs/research/capture-rotation-material-shimmer-20261104.md`.

### Four shared capture slots moved waits; no demonstrated emulator FPS win
- **Date**: 2026-11-04
- **Failure**: restaurant rotationON reduced the per-slot reader wait but raised
  xrEndFrame time; cadence stayed about60/s. Later rotationOFF world phases also
  had small reader waits. Menu-only reverse controls cannot validate gameplay.
- **Next**: retain defaultOFF, per-slot fences and pixel/reset tests; require
  equivalent clean gameplay and headset evidence before enabling. Do not call
  lower time in one overlapping scope recovered FPS. Same research report.

Searchable historical evidence index. Add entries when an approach fails.
Legacy lessons describe their original experiments, not current renderer
contracts or permission to modify native registers. Revalidate against current
source, BASELINE.md and the task handoff before applying them.

### Simulator screenshots and context polls are not clean CPU/FPS evidence
- **Date**: 2026-11-03
- **Branch**: codex/simulator-perf-shadow-20261103
- **Failure**: Screenshot/readback produced a single891ms hitch; a second
  screenshot/movement/cinema interval measured59fps. Neither is a clean baseline
  for the user's sustained40fps issue. Wall-clock IP snapshots include waits
  and can perturb timing; they are not ETW CPU execution samples.
- **Blocker**: WPR policy privilege0xc5585011 persisted outside sandbox. No
  valid ETL was obtained. Repeating the identical command will not grant admin.
- **Next**: Separate no-capture cadence and native critical-path attribution;
  see HANDOFF-simulator-perf-shadow-20261103.md for exact private evidence.

### Exact source3200x3264: real allocation, rejected performance
- **Date**: 2026-11-03
- **Branch/Commit**: codex/source-resolution-3200-20261103 /6d696052,d15b9bc5
- **Failure**: User reports FPS loss, CPU frame time around25ms, shimmering
  hands and head-relative floor reflections; also reports shadows affected.
  NativeSourceTarget log confirms actual3200x3264 RT0 (not an ignored request).
  IsolatedEyeOutput remained active; NIS timings alone do not explain the full
  CPU frame. Do not claim the extra pixels caused every reported artifact.
- **Action**: Closed-game rollback first returned D178DF2D/5A5BFDE9, then exact
  accepted Combined+IsolatedEyeOutput E56BF027/68EE5685 using frozen packages.
  Both installed hashes and four protected targets independently verified.
  No launch. User A/B on restored baseline pending.
- **Evidence**: HANDOFF-render-artifacts-cinema-20261103.md; original private log
  test-output/source-resolution-rejected-20261103/d3d9.log and user MP4.

### Scoped world diffuse AF16: no visual benefit, severe user performance regression
- **Date**: 2026-11-03
- **Branch/Commit**: codex/anisotropic-16x-20261103 /b4ed0fa8,c2162522
- **Approach**: Existing WorldDiffuseAnisotropy1 promotes LINEAR diffuse sampler0
  only for VS293F456B/PSCA08BDC4, with TextureAnisotropy16. DLL unchanged.
- **Failure**: User reports shimmer unchanged and90->40FPS. Captured log records
  world-bearing submission cadence roughly38.8-53/s in later windows; that is
  not an independent simulation-FPS measurement or isolated GPU-cost proof.
- **Action**: MAIN closed-game exact Restore PASS, DLL D178DF2D and pre-trial
  INI5A5BFDE9; FXAA remains0, NIS/center/resolution retained. Protected hashes
  independently verified. Do not re-enable automatically. Existing479-check
  sampler/state-restore fixture PASS was correctness evidence, not FPS proof.
- **Evidence**: HANDOFF-anisotropic16-20261103.md; private preserved runtime log
  test-output/anisotropic16-rejected-20261103/d3d9.log. Next performance inquiry
  must separate per-draw driver queries/state churn from filtering GPU cost.

### Running cinematic pointer alone falsely enables villa theater
- **Date**: 2026-11-02
- **Branch/Commit**: codex/villa-cinema-hands-audio-20261102 (candidate)
- **Failure**: RegionMgr+268 can remain nonnull during free walking/NPC animation;
  user reports unwanted theater and stiff hands in villa. Pointer nonnull alone
  does not establish native camera takeover.
- **Correction 2026-11-03**: The earlier +7BC label was wrong: it is
  mCanDamagePlayer. Pinned serializer identifies mFreeCam at +7BD and mOnRails
  at +7BE, on Cinematic vtableFCA7E0. See cinema-control-flags-20261103.md.
  Require IsPlaying and neither FreeCam nor OnRails for passive theater.
  Native fields and candidate evidence in HANDOFF-villa-cinema-hands-audio-20261102.md;
  physical villa acceptance remains pending. Do not substitute movement heuristics.

### BinkWait registry is not fullscreen movie ownership
- **Date**: 2026-11-02
- **Branch/Commit**: codex/villa-cinema-hands-audio-20261102 /44d9e7ba
- **Failure**: User still gets cinema while free-walking in villa. Live log mode2
  frame4322 native0/video1 and read-only registry confirm Bink path. FreeCam fix
  does not cover this. Wait/Close lifetime identifies codec users, not presentation.
- **Next**: Find native full-screen movie owner/presentation path. Do not use
  arbitrary dimensions, missing-frame timeout or blanket world-camera exclusion.
  Read HANDOFF-villa-cinema-hands-audio-20261102.md follow-up for callsites/handles.

## Format
```
### [Title]
- **Date**: YYYY-MM-DD
- **Branch/Commit**: [...]
- **Goal**: [...]
- **Approach**: [...]
- **Why Failed**: [...]
- **Evidence**: [logs, addresses, hashes, file refs]
- **Lesson**: [...]
- **Status**: Failed | Rejected
```

## Known Failures (from existing docs)

### View/hand camera stabilizer and per-frame heap shake suppression
- **Date**: 2026-11-01
- **Approach**: `CameraAnimStabilize` (post-hoc view/hand correction from avatar parent) and `CameraShakeSourceSuppress` (zero factors/impact enables on all 00FDF170 owners each frame).
- **Why Failed**: game writes the camera after the correction is computed (one frame late every frame); factors are rewritten ~every frame (refills); user verdict "tragedia".
- **Status**: Rejected; see HANDOFF-camera-shake-stabilizer-20261101.md

### Heap-only procedural shake factors on the 00FDF170 owner
- **Date**: 2026-11-01
- **Goal**: Remove first-person camera shake without EXE writes.
- **Approach**: `CameraShakeSuppressHeap=1` zeroed `mShakeFactorRot`/`mShakeFactorPos` of blocks `8C0/910/960/CA0` on the owner reached by `01001630 avatar +0x5E0 -> 00FDF170`.
- **Why Failed**: It ran (`ACTIVE=1`, 2602 writes, 0 refills, 0 refusals), but over 30 s of running and action the owner's generator amplitudes, outputs, impact springs `+0x1170` and accumulators `+0x1F0/+0x20C` were all exactly zero. That owner is not driving the visible shake. The user still saw shake everywhere. Same-run trace: camera relative to avatar root moved up to ~0.1 units while running and was steady when standing still, which points to animated camera/head bob rather than this procedural system.
- **Evidence**: commit `146f8dfc`, DLL `FB370181C19676C4151D076D52FFA3CA2D5DD4F6C1E63D987ADA77B0A39CBBA4`, INI `B42211412D5F6D94147C652679111A6B8B49A60DDAFB51B8293B89BCF6F9B3A1`, package `camera-shake-heap-20261101T194916318`, `[CameraShakeHeap]` lines in `d3d9.log`.
- **Lesson**: A verified type chain does not prove a live owner. Check that the target fields actually change during the symptom before writing them.
- **Status**: Rejected

### Procedural shake call-site patch under protected image guard
- **Date**: 2026-11-01
- **Branch/Commit**: `codex/gaze-nis-main-integration` / `d1ed1aa9`
- **Goal**: Suppress first-person procedural camera shake while preserving the native hand and weapon transform order.
- **Approach**: With `CameraShakeSuppress=1`, redirect four pinned EXE calls to generator `00628420`, run the original, then clear its output vector/scalars.
- **Why Failed**: The installed profile has `BlockNativeImagePatches=1` and `RestoreTrackedGameplay=0`. The startup log recorded `requested=1` and `[NativeImageGuard] enabled`, but never `active=1`. The user reported that the camera still shook. This was an admission failure, not evidence that the generator does not contribute to shake.
- **Evidence**: Candidate DLL `CC53C2E4D0EFEFAA3E713B0E3404148F07DF20BE61363AFEFC0D3D40B6992EB1`; diagnostic INI `7E1160EF7C634EFFBD356AE21CCF7EB44F28C0218E633A062EDD2AF19D179687`; runtime log at `%LOCALAPPDATA%/Darkness2VR/d3d9.log`. Protected rollback package `camera-shake-suppress-20261101T180426583` restored all nine targets; independent 9/9 hash audit verified prior DLL `EF58E6F6BEDEADF929F9021E562ED33C5D9A806E341CDEB950775D7C1A878567` and diagnostic INI `30EDBFD53636856E57312375983749D9FB5D8ABB8548E43EE837F0D4782988FD`.
- **Lesson**: Do not propose EXE call-site patches for the protected profile. Locate a selected, heap-owned live object and use a verified per-instance route or another admission-compatible intervention; prove activation in the log before evaluating visual effect.
- **Status**: Rejected

### Shotgun model-only protected pose without native fire coupling
- **Date**: 2026-11-01
- **Branch/Commit**: `codex/gaze-nis-main-integration` / `eb00b9d6`
- **Goal**: Point the equipped shotgun model with the right controller while preserving its native fire action.
- **Approach**: Publish the shotgun's owned model vtable through the generic controller-pose bridge, and align the protected right human hand to the same aim pose. Leave shotgun fire action `00FD9D68` untouched.
- **Why Failed**: User saw the shotgun point forward but could no longer hit targets; fingers also failed to reach the trigger. The native fire action apparently consumes model-derived state or another transform coupled to it. This is an inference from the A/B headset result, not a traced fire call.
- **Evidence**: Installed trial DLL `5389DF98690B1A4D67E4585329E1DD525DE257C7F1E95E0B187A6DA5B7101BDF`; fresh `[WeaponPose] shotgun=1` logs and user screenshot `codex-clipboard-9c2f1f89-5202-4c25-9790-4f5fdbb07067.jpg`; `HANDOFF-shotgun-twohand-shake-20261101.md`. Protected rollback restored prior DLL `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`, all nine targets verified.
- **Lesson**: Do not steer this shotgun model through the generic weapon matrix before tracing its native shot origin/direction and hand socket. Model and shot require a proven shared frame or a display-only rendering adjustment outside native gameplay state.
- **Status**: Rejected

### Bone matrix corruption from scanning all shader constants
- **Date**: ~2026-10-18 (from handoffs)
- **Source**: AGENT_HANDOFF_PROMPT.md §4A
- **Approach**: Scan vertex shader constants up to register 256 looking for 4x4 matrices by invariants
- **Why Failed**: Matched skeletal bone palettes (c45+, c52+) - hands/weapons/enemies spiked/distorted
- **Evidence**: "Vector4fCount == 168 or 75", matched 13+ bone matrices per frame
- **Lesson**: Lock to StartRegister == 0 only (c0..c3). Never scan bone palette ranges.
- **Status**: Failed

### Modifying c0..c3 only in deferred renderer
- **Date**: ~2026-10-18
- **Source**: AGENT_HANDOFF_PROMPT.md §4B
- **Approach**: Translate camera via SetVertexShaderConstantF c0..c3
- **Why Failed**: G-buffers/deferred lighting/cel-shading decoupled. Multi-constant uploads (c0 has 12-17 vectors). AFR temporal mismatch causes retinal rivalry.
- **Evidence**: Deferred rendering artifacts, flat image or desync
- **Lesson**: Need native camera hook or DIBR/depth-based or full geometry stereo (SubmitMode 4 same-frame). c0..c3 shear alone insufficient for deferred.
- **Status**: Failed

### Hooking SwapChain::Present
- **Date**: 2026-10-18 (HANDOFF.md v0.5)
- **Source**: HANDOFF.md §3 stability
- **Approach**: Hook IDirect3DSwapChain9::Present vtable
- **Why Failed**: Not at slot 3 in runtime vtable, broke presentation/stopped render
- **Evidence**: Frame boundary = EndScene (VrFrame). Hooking objects outside device without verification fails.
- **Lesson**: Verify vtable slots at runtime. Use EndScene as frame boundary.
- **Status**: Failed

### UpdateTexture with dead textures during replay
- **Date**: 2026-10-18
- **Source**: HANDOFF.md §3
- **Approach**: Replay all device calls including UpdateTexture
- **Why Failed**: NVIDIA driver throws 0xC0000002 with dead textures, crashes
- **Evidence**: Game creates/frees textures mid-frame
- **Lesson**: SEH guard on replay records, skip UpdateTexture with dead textures, SafeMemcpy on capture.
- **Status**: Failed

### AFR (Alternate Frame Rendering)
- **Date**: ~2026-10-18
- **Source**: AGENT_HANDOFF_PROMPT.md
- **Approach**: Alternate frames between eyes
- **Why Failed**: Temporal disparity at 60/90Hz causes retinal rivalry, can't fuse with head movement
- **Evidence**: User rejected
- **Lesson**: Same-frame stereo required (SubmitMode 4)
- **Status**: Rejected

### Engine FOV override via avatar vtable (early attempts)
- **Date**: 2026-10-21 (MODLOG RC10/RC11)
- **Approach**: Patch GetFOV/avatar paths
- **Why Failed**: CPU cull happens before GPU widen; GPU-only can't resurrect culled meshes. Need engine-level FOV sync.
- **Evidence**: RC10 shows GPU-only miss, need Scaled_GetFov/engine sync
- **Lesson**: Sync engine CPU cull (GetFOV/global) separately from GPU projection
- **Status**: Failed/Iterating

### Hand palette patch at 66B3CA (RC8)
- **Date**: 2026-10-21
- **Source**: MODLOG
- **Approach**: Patch renderer palette build site
- **Why Failed**: Crashed on startup (E129EA memcpy AV). Incompatible before callback execution
- **Evidence**: test-output/rc8-startup.log
- **Lesson**: Quarantine unsafe hooks, test startup before in-level. RC7b removed it.
- **Status**: Rejected

### Auxiliary scene suppression heuristic
- **Date**: 2026-10-18
- **Source**: MODLOG Investigation
- **Approach**: Skip auxiliary EndScene windows
- **Why Failed**: User reported heavy flicker "wszystko miga ciezko cokolwiek zanotowac"
- **Evidence**: Rejected, reverted to 72DD...
- **Lesson**: Don't guess frame scheduling; need captured gameplay evidence
- **Status**: Rejected

### Named projection fix (B36A...)
- **Date**: 2026-10-18
- **Source**: MODLOG
- **Approach**: CTAB-based named matrix lookup for lighting correction
- **Why Failed**: Duplicate NPC/shadow silhouettes in both eyes, zoomed image, not convincing VR
- **Evidence**: User rejection
- **Lesson**: Experimental fixes must be behind flags, easy to disable
- **Status**: Rejected


### Adaptive half-rate application limiter: user rejected (2026-10-31)
- **Approach**: AUTO application pacing to half physical headset refresh, with uncapped recovery probes.
- **Evidence**: source 346fac93; log selected HALF at 120 Hz (~58 app FPS), but user reported it does not work and requested OFF.
- **Boundary**: app limiting does not establish or force runtime frame synthesis. Do not describe cap transitions as reprojection proof.
- **Status**: Disabled in Steam/local primary profiles; repo default OFF. See HANDOFF-adaptive-half-rate.md and BASELINE.md for rollback/identities.
# 2026-11-01: rejected comic texture storage and incomplete Polish font coverage

- `polish-comic-main-20261101-accepted-camera` installation had valid hashes but user reports rainbow artifacts on hands/tentacles and missing Polish caption letters.
- Comic manifest `eb915af2...`: planar compressed-layout BC1/BC3 data was installed as uncompressed entries. Native uncompressed StreetSignsA uses standard BC blocks; direct decoding reproduces the candidate's noise. Fix the storage layout before reinstalling; a self-roundtrip and checksum alone cannot validate native interpretation.
- Font pack patched six families only, leaving19 Latin font families without Polish codepoints. Compressed native BC2 uses four uint32 planes per16KB chunk, not one contiguous eight-byte alpha plane. Original texture dimensions must come from headers (Corbel26 is1024x64).
- Evidence and repair: `handoffs/HANDOFF-texture-font-repair-20261101.md`. Preserve the accepted camera/haptics/model DLL and updated audio/text while repairing assets.
# 2026-11-01: font atlas enlargement failed physical acceptance

- `texture-font-repair-v3` fixed comic texture artifacts (user accepted) but made gameplay/title text fragment. Its font strategy enlarged19 atlases and rescaled original English V coordinates. Offline glyph proofs and metrics checks were insufficient to validate native rendering. Exact native mismatch is not yet proven.
- Replacement candidate must preserve original atlas dimensions, UV bytes, metrics and original glyph pixels; place new glyphs only in unused space. Do not roll back the now-accepted comic texture archives with the fonts. See `HANDOFF-texture-font-repair-20261101.md`.

### Tiny stereo-matrix preparation worker: benefit gate not passed (2026-11-03)
- **Approach**: one persistent CPU worker prepares owned same-frame WVP/STP matrices; exact input/parameter/FP keys admit ready output, otherwise serial fallback.
- **Evidence**: actual-source correctness fixtures PASS, but optimistic ready-output-only consumption cost was approximately70.6ns serial/72.4ns cached and41.9ns/50.5ns in separate synthetic runs, before any prepass, queue or join cost. These are not gameplay frame times or FPS results.
- **Decision**: default OFF; test candidate uses shader cache alone. Do not present thread utilization or correct output as a performance win. Keep explicit Worker/Both modes only for bounded experiments; a larger proven independent producer or materially lower preparation cost is needed before default admission.
- **Details**: `docs/research/performance-candidate-20261103.md`, `handoffs/HANDOFF-performance-candidate-20261103.md`.
### SMAA1x HIGH after NIS: no headset shimmer benefit
- Date: 2026-11-04. Source33fe966d, installed DLL2119EA3C.
- User accepts scene/input behavior but reports SMAA does not resolve edge shimmer.
- GPU/flat/diagonal and simulator checks were correctness evidence, not temporal
  or headset effectiveness. Do not repeat a generic post-filter as a proven fix.
- Follow-up found a separate exact issue: missing gaze switches localized NIS
  to uniform80% strength. Actual-source pixel regression FAIL before center
  fallback, PASS after; not proof that all spatial/specular aliasing is fixed.

### 2026-11-04: base-only font repair misses DLC override
- Full native font alias in base H/B.Font and menu-font reference experiment
  did not correct objective glyphs in emulator; both fully rolled back.
- Cache.DLC/PCPatchManifests/Cache.Windows supplies25 overriding H/B.Font pairs.
  All18 PL UVs differ from repaired base definitions (non-PL UVs match), while
  English glyph textures resolve to the base atlases. Synchronize the overlay;
  do not repeat base-only font/atlas edits as a complete fix.
- Evidence HANDOFF-vr-settings-polish-fonts-20261104.md, dlc-font-comparison.json,
  emulator polish-menu-zoom.png vs dlc-font-after.png.

### 2026-11-04: native outline footprint and isolated shadow-mask comparison
- Quarter-pixel footprint temporal ratio0.8661 failed20% GPU fixture gate;
  half-pixel four-sample native coverage passed0.5879 without full-colour blur.
- Linear normal sampling worsened native outline variation1.1838; reverted.
- C4D1FB10 after-draw RGB is accumulated lighting, not the isolated shadow mask.
  Comparing grayscale/dark RGB gave invalid optimistic scores. Require before
  target plus actual blend contract; DESTCOLOR/ZERO recovers after/before.
- Scoped shadow selector must admit configured lighting profile-1, not only0.
  Verify actual consuming c223; driver shader creation alone did not prove ON.
- PS3 LRP with destination aliasing its first source was driver-rejected; final
  independent r14 selector/r15 result passes. Per-light/cinema/headset coverage
  still incomplete. See HANDOFF-edge-shadow-stability-20261104.md.
