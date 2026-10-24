# Task branch engineering log

## 2026-10-24 — Offline native camera/FOV follow-up

On `codex/native-camera-fov`, based on research `cb6638f`, verified different
Lua receiver type globals for GetFOV and the base-FOV setter. Identified the
FCOS-to-dot-product candidate filter, perspective-shaped constructors
`00A31FF0` / `00DF3B50`, and eight-corner constructor `00DF53E0` with Ghidra
and static instruction evidence. Active player projection and CPU visibility
producer/consumer are still open; no native hook was implemented or deployed.

Preserved the private Ghidra project outside Git. Added the existing read-only
PE audit and its six passing parser tests to version control, guarded explicit
Ghidra entry assertions with original bytes, and prepared a disabled observation
manifest. The depot sample is rejected by the audit's installed-SHA policy even
though all six instruction sites match. Steam/PSVR2 testing waits for the user
to resolve the antivirus detection and safely restore the installation.

Detailed evidence and limits: [native camera audit](docs/research/native-camera-fov-audit.md).


## MAIN 2026-10-24 - clean offline integration and opt-in matched capture

Approved picks: 744e637, accd9b2, f978db8, 5f59fe1, ddaf4af, in that order.
New dedicated integration worktree; original untracked MODLOG preserved.
Added compile/runtime-off one-frame RT0/final-eye capture with draw banks,
metadata, completeness receipts and checked Luna analysis. No Gemini adapter.
No production INI change or deployment. Ordinary preprocessed production source
matches the integrated baseline. Native camera/CPU visibility ownership remains
open; static worker/dispatcher/bounds evidence and reproducible range hashes
added. Restored installed EXE passes identity/14-site/10-range audit, but AV
resolution and native-patch-free Present observation still block runtime gates.
Details and validation: docs/research/matched-frame-capture.md.
