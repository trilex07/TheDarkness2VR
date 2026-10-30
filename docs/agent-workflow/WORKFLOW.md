# Workflow

## Roles
- **MAIN**: Plans, coordinates, reviews, merges. Owns baseline and handoffs.
- **SIDE**: Implements in isolated worktree/branch. Reports short progress, stores evidence in files.

## Isolation
- Use dedicated git branch per task: `agent/<task-name>-<short-id>`
- Use git worktree for parallel work: `git worktree add ../Darkness2VR-<task> <branch>`
- Never work directly on `main` for experimental changes
- Keep changes scoped to task

## Git Discipline
- Commit often with clear messages
- Reference evidence (addresses, logs, hashes) in commits
- Push branches to remote
- Review diffs before merging
- Update `BASELINE.md` only when change is accepted and tested
- Use timestamped backups for DLL/INI installs (build.ps1 does this)

## Handoffs
- Keep `HANDOFF-CURRENT.md` compact (goal, status, files, next steps, blockers)
- Move detailed analysis to appropriate docs (RE_FINDINGS, etc.) or task notes
- Include: baseline commit, branch, worktree, evidence refs, failed attempts tried
- Fresh agent must resume from git + handoff + baseline without full chat history

## Regression Gates
Before merging/accepting:
- [ ] Builds successfully (`build.ps1`)
- [ ] No crashes on startup (check log)
- [ ] Preserves working features (check against baseline)
- [ ] Steam exclusive access respected during tests
- [ ] Evidence documented (addresses/bytes if patching)
- [ ] Rollback path verified (backup exists)
- [ ] Committed and pushed

## Steam Exclusivity
- Only one agent/session tests in-game at a time
- Close game before installing new DLL
- Don't inject input
- Check PIDs in logs if needed
- Document test results in handoff

## Token Efficiency
- Load minimal files
- Reference existing docs instead of copying
- Store large outputs (logs, disasm) in files, summarize
- Use FAILED-APPROACHES to skip known failures
- Short progress reports
