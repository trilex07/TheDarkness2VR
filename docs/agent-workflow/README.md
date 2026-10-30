# Agent Workflow for Darkness2VR/SingularityVR VR Mods

Reusable, token-efficient workflow for VR mod development.

## Quick Start
1. Read `AGENTS.md` (root)
2. Read `BASELINE.md` for accepted state
3. Read/create `HANDOFF-CURRENT.md` for active task
4. Check `FAILED-APPROACHES.md` before trying ideas
5. Follow `WORKFLOW.md` for process

## Files
- `WORKFLOW.md` - Roles, worktrees, Git, gates, Steam rules
- `BASELINE.md` - Accepted baseline tracking
- `HANDOFF-TEMPLATE.md` - Compact handoff template
- `handoffs/` - Per-task handoffs only (create `HANDOFF-<task-id>-<branch>.md` from template)
- `FAILED-APPROACHES.md` - Searchable failed approaches index
- `TASKS/` - Reusable templates
- `SCRIPTS/` - Validation scripts (optional)

## Principles
- Git is source of truth
- Evidence before patch (address + bytes/instructions)
- Minimal context per task
- Detailed evidence in files, short progress reports
- Isolated branches/worktrees
- Regression gates, exclusive Steam access
- Rollback via timestamped backups
- Reuse existing docs, avoid duplication
