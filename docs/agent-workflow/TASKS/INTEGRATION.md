# Task Template: Integration

Start with root `AGENTS.md`; run read-only bootstrap with your actual MAIN/SIDE role.
Use `handoffs/HANDOFF-<task-id>.md`; record the actual Git branch inside it.
SIDE performs offline work only and never modifies or launches installed games.

## Goal
[Integrate feature/change safely]

## Checklist
- [ ] Compatible with current baseline
- [ ] Config options added if needed
- [ ] Defaults preserve working behavior
- [ ] Relevant offline checks pass; installation only by authorized MAIN for runtime changes
- [ ] Runtime smoke by authorized MAIN if relevant; N/A for documentation-only changes
- [ ] Committed and pushed
- [ ] Handoff updated
