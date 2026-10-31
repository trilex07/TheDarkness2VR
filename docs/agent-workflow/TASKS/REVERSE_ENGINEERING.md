# Task Template: Reverse Engineering

Start with root `AGENTS.md`; run read-only bootstrap with your actual MAIN/SIDE role.
Use `handoffs/HANDOFF-<task-id>.md`; record the actual Git branch inside it.
SIDE performs offline work only and never modifies or launches installed games.

## Goal
[Find address/function/structure]

## Rules
- **Evidence before patch**: address + bytes/instructions that prove it
- Never guess offsets/registers
- Document in RE_FINDINGS.md or appropriate file
- Don't apply unverified patches

## Plan
1. Define question/hypothesis
2. Gather evidence (disasm, xrefs, runtime logs)
3. Verify with multiple sources
4. Document findings
5. Only patch if verified and safe

## Checklist
- [ ] Evidence recorded (VA, bytes, disasm)
- [ ] Cross-referenced
- [ ] Documented in repo
- [ ] No guessing
- [ ] Tested if patched
