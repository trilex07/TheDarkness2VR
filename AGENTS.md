# Darkness2VR Agent Guide (Root)

**Mandatory entry point:** every agent session must read this file before project work. Explicit user/session instructions take precedence. Keep context minimal - read only what you need.

## Quick Start (Resume a Task)
1. Run `docs/agent-workflow/bootstrap.ps1 -Role SIDE -Task <task-id>` (MAIN uses `-Role MAIN`) for a read-only baseline/branch/task summary
2. Read `docs/agent-workflow/BASELINE.md` - accepted Steam binary, verified source commit, integration branch
3. Read your **per-task handoff** from `docs/agent-workflow/handoffs/HANDOFF-<task-id>.md` (create from `HANDOFF-TEMPLATE.md`)
4. Check `docs/agent-workflow/FAILED-APPROACHES.md` for relevant failures
5. Read only the specific evidence files needed (see references below)
6. Reuse the assigned task worktree/branch; do not modify another agent's worktree. Create a new worktree only when needed and authorized by the session workflow.

## Source of Truth
- **Git**: All decisions, evidence, handoffs live in repo. Commits are required.
- **Baseline**: `docs/agent-workflow/BASELINE.md` tracks accepted Steam binary, verified source commit, integration branch (not assumed to be Git `main`).
- **Handoffs**: **Per-task only** in `docs/agent-workflow/handoffs/`. Use `docs/agent-workflow/HANDOFF-TEMPLATE.md`. Never use a shared active-task handoff.
- **Failures**: `docs/agent-workflow/FAILED-APPROACHES.md` - search before trying known-bad approaches.
- **Task IDs**: unique, stable ASCII IDs matching `[A-Za-z0-9][A-Za-z0-9_-]{0,63}`; no branch suffix in the filename. Keep the actual, unsanitized Git branch in **Branch** inside the document. Bootstrap rejects unsafe IDs instead of silently aliasing them.
- **Detailed evidence (historical, not current task instructions)**: `HANDOFF.md`, `MODLOG.md`, `AGENT_HANDOFF_PROMPT.md`, `ENGINE_REVERSE_ENGINEERING.md`, `RE_FINDINGS.md`, `RE_CULL_EVIDENCE.md` - read as needed.

## Project Facts (Preserve)
- Game: The Darkness II (Steam AppID 67370), x86 D3D9Ex, Evolution Engine
- Proxy: `d3d9.dll` built via `build.ps1` (MSVC x86), OpenXR bridge
- Deployment: **MAIN ONLY**, when authorized, with session availability, exact DLL/EXE/profile verification and full DLL/INI/cache rollback. `build.ps1 -Install` backs up only the old DLL and also copies repo configuration; it is not a complete protected-profile rollback procedure. Use the current task deployment package/instructions.
- Config: `Darkness2VR.ini` (repo root + game dir)
- Logs: `%LOCALAPPDATA%\Darkness2VR\d3d9.log`
- Current focus: SubmitMode=4 same-frame stereo (record+replay). AFR rejected. FOV/culling critical.
- Rules: Evidence before patch (address + bytes/instructions). Never guess offsets/registers. **MAIN ONLY: Steam launch, deployment, runtime testing. SIDE: offline only.** Rollback via backups.

## Workflow
See `docs/agent-workflow/WORKFLOW.md` for MAIN/SIDE roles, worktrees, Git discipline, per-task handoffs, gates, Steam exclusivity.

## Task Templates
`docs/agent-workflow/TASKS/` - IMPLEMENTATION, REVERSE_ENGINEERING, PERFORMANCE_OPTIMIZATION, INTEGRATION

## Token-Saving
- Don't re-read entire chat history - resume from git + task handoff + baseline
- Load minimal files per task
- Put detailed evidence in files, keep progress reports short
- Use FAILED-APPROACHES index to avoid repeats
- Reuse existing docs, don't duplicate
- Per-task handoffs prevent loading unrelated context
