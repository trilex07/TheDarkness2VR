# Failed Approaches Index

Searchable historical evidence index. Add entries when an approach fails.
Legacy lessons describe their original experiments, not current renderer
contracts or permission to modify native registers. Revalidate against current
source, BASELINE.md and the task handoff before applying them.

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
