# Task Template: Performance Optimization

Start with root `AGENTS.md`; run read-only bootstrap with your actual MAIN/SIDE role.
Use `handoffs/HANDOFF-<task-id>.md`; record the actual Git branch inside it.
SIDE performs offline work only and never modifies or launches installed games.

## Goal
[Optimize without breaking functionality]

## Focus Areas
- CPU overhead (capture/replay)
- Memory usage
- Frame timing
- Avoid extra copies

## Rules
- Measure before/after
- Don't break stereo
- Preserve rollback

## Checklist
- [ ] Applicable retained baseline evidence identified; avoid unnecessary repeated baseline runs
- [ ] No functional regressions
- [ ] Runtime result recorded by authorized MAIN, or explicitly NOT RUN at the SIDE gate
- [ ] Evidence documented
- [ ] Committed
