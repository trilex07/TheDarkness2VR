# MinHook x86 subset

Unmodified local sources copied from
`SingularityVR-production/third_party/minhook` on 2026-10-26. Only the x86
implementation is built, and only with `build.ps1 -DiagnosticCapture`.
Upstream: https://github.com/TsudaKageyu/minhook
License: [LICENSE.txt](LICENSE.txt). Original source notices are retained.

This local copy does not include an upstream version tag; do not infer one.
Provenance pins before integration:

- `src/hook.c`: `c6eec4f8dc5fb6513347c3f44fbdb25ca0bba7a2bcbe141d238c7da97094cd9d`
- `include/MinHook.h`: `d5cf8db333895134398c7f69bac43f232835fda1cccbd86875c654cd69989b23`

The caller admits executable method entries in external image modules only.
MinHook may relocate an existing JMP to an overlay. Each target owns its own
trampoline; the caller must never substitute a freshly read live vtable slot
for that trampoline. The main EXE image is not a hook target.
