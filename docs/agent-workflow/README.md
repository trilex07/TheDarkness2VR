# Agent Workflow for Darkness2VR

Reusable, token-efficient workflow for VR mod development.

## Quick Start
1. Read `AGENTS.md` (root)
2. Read `BASELINE.md` for accepted state
3. Read/create `handoffs/HANDOFF-<task-id>.md` for your task; keep the actual Git branch inside it
4. Check `FAILED-APPROACHES.md` before trying ideas
5. Follow `WORKFLOW.md` for process

## Files
- `WORKFLOW.md` - Roles, worktrees, Git, gates, Steam rules
- `BASELINE.md` - Accepted baseline tracking
- `HANDOFF-TEMPLATE.md` - Compact handoff template
- `handoffs/` - Per-task handoffs only (create `HANDOFF-<task-id>.md` from template)
- `FAILED-APPROACHES.md` - Searchable failed approaches index
- `TASKS/` - Reusable templates
- `bootstrap.ps1` - Read-only session summary
- `test-bootstrap.ps1` - Isolated offline bootstrap regression

## Principles
- Git is source of truth
- Evidence before patch (address + bytes/instructions)
- Minimal context per task
- Detailed evidence in files, short progress reports
- Isolated branches/worktrees
- Regression gates, exclusive Steam access
- Rollback via timestamped backups
- Reuse existing docs, avoid duplication

## Bootstrap checks
Run `powershell -NoProfile -ExecutionPolicy Bypass -File docs/agent-workflow/bootstrap.ps1 -Role SIDE -Task your-task-id`
(or `-Role MAIN` for MAIN). It only reads the assigned repository and baseline
snapshot. It never creates a handoff, installs files, accesses Steam or grants
permissions. Task IDs use `[A-Za-z0-9][A-Za-z0-9_-]{0,63}`; unsafe IDs are
rejected rather than sanitized into potentially colliding names. Names are
case-insensitive on Windows: case variants are the same task ID. Store the
actual Git branch, including slashes, inside `HANDOFF-<task-id>.md`.

Offline regression: `powershell -NoProfile -ExecutionPolicy Bypass -File docs/agent-workflow/test-bootstrap.ps1`.
It creates only a synthetic TEMP repository, leaves assigned/other worktrees
and installed games untouched, and checks read-only MAIN/SIDE operation.
