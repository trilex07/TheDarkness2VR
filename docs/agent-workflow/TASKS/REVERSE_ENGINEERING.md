# Task Template: Reverse Engineering

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
