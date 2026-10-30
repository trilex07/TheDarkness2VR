# Darkness2VR Agent Guide (Root)

This is the entrypoint for fresh agent sessions. Keep context minimal - read only what you need.

## Quick Start (Resume a Task)
1. Read `docs/agent-workflow/BASELINE.md` - accepted baseline (commit, DLL hash, INI, game state)
2. Read `docs/agent-workflow/HANDOFF-CURRENT.md` if present, else create from template
3. Check `docs/agent-workflow/FAILED-APPROACHES.md` for relevant failures
4. Read only the specific evidence files needed (see references below)
5. Work on a dedicated branch/worktree per task

## Source of Truth
- **Git**: All decisions, evidence, handoffs live in repo. Commits are required.
- **Baseline**: `docs/agent-workflow/BASELINE.md` tracks accepted state.
- **Handoffs**: `docs/agent-workflow/HANDOFF-CURRENT.md` (compact). Use `docs/agent-workflow/HANDOFF-TEMPLATE.md`.
- **Failures**: `docs/agent-workflow/FAILED-APPROACHES.md` - search before trying known-bad approaches.
- **Detailed evidence**: `HANDOFF.md`, `MODLOG.md`, `AGENT_HANDOFF_PROMPT.md`, `ENGINE_REVERSE_ENGINEERING.md`, `RE_FINDINGS.md`, `RE_CULL_EVIDENCE.md` - read as needed.

## Project Facts (Preserve)
- Game: The Darkness II (Steam AppID 67370), x86 D3D9Ex, Evolution Engine
- Proxy: `d3d9.dll` built via `build.ps1` (MSVC x86), OpenXR bridge
- Install: `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install` (backs up to `d3d9.dll.off-<timestamp>`)
- Config: `Darkness2VR.ini` (repo root + game dir)
- Logs: `%LOCALAPPDATA%\Darkness2VR\d3d9.log`
- Current focus: SubmitMode=4 same-frame stereo (record+replay). AFR rejected. FOV/culling critical.
- Rules: Evidence before patch (address + bytes/instructions). Never guess offsets/registers. Exclusive Steam access during testing. Rollback via backups.

## Workflow
See `docs/agent-workflow/WORKFLOW.md` for MAIN/SIDE roles, worktrees, Git discipline, regression gates, Steam exclusivity.

## Task Templates
`docs/agent-workflow/TASKS/` - IMPLEMENTATION, REVERSE_ENGINEERING, PERFORMANCE_OPTIMIZATION, INTEGRATION

## Token-Saving
- Don't re-read entire chat history - resume from git + handoff + baseline
- Load minimal files per task
- Put detailed evidence in files, keep progress reports short
- Use FAILED-APPROACHES index to avoid repeats
- Reuse existing docs, don't duplicate
