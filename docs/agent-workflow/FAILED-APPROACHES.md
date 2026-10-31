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

### `git worktree prune` from the Linux cowork VM
- **Date**: 2026-10-31
- **Branch/Commit**: side/cpu-hotpath-opt (workflow incident, not renderer code)
- **Goal**: Clean up one misplaced SIDE worktree from the Linux VM shell
- **Approach**: `git worktree prune` in the mounted Windows repo
- **Why Failed**: The VM cannot resolve `C:/...` gitdir paths, so git judged every Windows worktree "missing" and deleted all `.git/worktrees/<id>` admin folders (HEAD/index/reflog). Commits, branches and working files were untouched.
- **Evidence**: only `-side-cpu-hotpath` left in `.git/worktrees`; recovery script `repair-worktrees-20261031.ps1` (repo root, untracked)
- **Lesson**: Never run `git worktree prune/remove/add` on the Windows repo from the VM. Create SIDE worktrees inside the connected repo folder and use `GIT_DIR`/`GIT_WORK_TREE`, or let Windows git manage worktrees.
- **Status**: Failed (recovery script provided)
