# Workflow

## Roles
- **MAIN**: Plans, coordinates, reviews, merges. Owns baseline and handoffs. **ONLY MAIN performs deployment, Steam launch, runtime testing, and game installation.**
- **SIDE**: Implements in isolated worktree/branch. **NEVER launches Steam, never modifies installed game files, never runs in-game tests.** Reports short progress, stores evidence in files. Does offline validation only (build, static checks, analysis).

## Isolation
- Use dedicated git branch per task: `agent/<task-name>-<short-id>` or follow existing naming
- Use git worktree for parallel work: `git worktree add ../Darkness2VR-<task> <branch>`
- Never work directly on `main` for experimental changes
- Keep changes scoped to task
- **Per-task handoffs**: Each task gets its own handoff file in `docs/agent-workflow/handoffs/` (see below). Never overwrite a shared `HANDOFF-CURRENT.md`.

## Handoffs (Per-Task)
- Location: `docs/agent-workflow/handoffs/HANDOFF-<task-id>-<branch>.md`
- Use `docs/agent-workflow/HANDOFF-TEMPLATE.md` as base
- Include: baseline commit, branch, worktree, role (MAIN/SIDE), task scope, files, evidence refs, failed attempts, next steps, blockers
- Fresh agent must resume from git + its task handoff + baseline without full chat history
- MAIN creates/updates handoffs for deployment tasks; SIDE creates/updates handoffs for its task only

## Git Discipline
- Commit often with clear messages
- Reference evidence (addresses, logs, hashes) in commits
- Push branches to remote
- Review diffs before merging
- Update `BASELINE.md` only when change is accepted and tested (MAIN only)
- Use timestamped backups for DLL/INI installs (build.ps1 does this) - MAIN only

## Regression Gates
Before merging/accepting (MAIN):
- [ ] Builds successfully (`build.ps1`)
- [ ] Startup smoke passed (MAIN only, game closed during install)
- [ ] Preserves working features (check against baseline)
- [ ] Steam exclusive access respected during tests
- [ ] Evidence documented (addresses/bytes if patching)
- [ ] Rollback path verified (backup exists)
- [ ] Committed and pushed

SIDE offline checks:
- [ ] Builds successfully
- [ ] Static analysis/review complete
- [ ] Tests pass (unit/compile)
- [ ] No game files touched
- [ ] Handoff updated

## Steam Exclusivity & Access Control
- **MAIN ONLY**: Launch Steam, install DLL to game dir, run game, perform in-game testing
- **SIDE NEVER**: Launch Steam, copy to game dir, modify installed game files, run game
- Close game before installing new DLL (MAIN)
- Don't inject input
- Document test results in handoff (MAIN)
- Startup smoke tests separated from SIDE offline validation

## Token Efficiency
- Load minimal files per task
- Reference existing docs instead of copying
- Store large outputs (logs, disasm) in files, summarize
- Use FAILED-APPROACHES to skip known failures
- Short progress reports
- Per-task handoffs prevent loading unrelated context
