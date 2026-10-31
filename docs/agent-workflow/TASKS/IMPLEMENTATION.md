# Task Template: Implementation

Start with root `AGENTS.md`; run read-only bootstrap with your actual MAIN/SIDE role.
Use `handoffs/HANDOFF-<task-id>.md`; record the actual Git branch inside it.
SIDE performs offline work only and never modifies or launches installed games.

## Goal
[Implement feature/fix]

## Requirements
- [ ] Follow existing patterns in codebase
- [ ] Minimal changes
- [ ] Evidence if patching engine/game
- [ ] Preserve working features

## Plan
1. Read relevant code (minimal)
2. Check FAILED-APPROACHES
3. Design change
4. Implement
5. Build
6. Run relevant offline checks; MAIN owns any separately authorized runtime validation

## Checklist
- [ ] No VR runtime refactor
- [ ] Uses existing build system
- [ ] Respects Steam exclusivity
- [ ] Backups handled
- [ ] Committed to task branch
- [ ] Updated handoff
