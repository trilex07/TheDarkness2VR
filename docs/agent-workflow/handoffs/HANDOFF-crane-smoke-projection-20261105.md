# Crane highlight and smoke projection

Date:2026-11-05. Role:MAIN.
Branch:codex/crane-smoke-projection-20261105.
Worktree:C:/Users/Trilex/Darkness2VR/.main-camera-shake.
Main source beace8e3; installed candidate E8C8015E from72d54b85.
Ammo accepted by user; crane highlight still rejected. Main unchanged.

Status:in progress. Double projection confirmed in ten pinned particle VS
variants: c0 produces clip and c4 consumes clip. Exclude c4 from tracked-world
STP injection. Source regression, reflection-scope regression and protected-native
build PASS. Need after-fix gameplay and impact dust verification. Reflection
still unresolved; selected capture also records requested material's PS bank.

Read docs/research/crane-smoke-projection-20261105.md and FAILED-APPROACHES.
Private lab test-output/crane-smoke-20261105: user selected Continue, gameplay
at crane. Own PID1636, exact ticks/path in process.json. Capture910/56046
isolates PSDA655BE2;911 selects crane PSD94B61DC. Fresh saves and E8 rollback
preserved. Before ending close only recorded identity-matched PID; restore
exact E8 DLL/INI and fresh user files. Record final recovery below.

Follow-up: implementation commit0c26dfad. Final protected-native build
5C55422FDFF8B599EA76964BD9883378A2DC2318405AE4B12970071CD1D2FD6F,
installed ONLY in the temporary simulator lab via Update. Normal baseline
rollback remains E8C8015E, exact original diagnostic INI and fresh saves.
New own PID26984, ticks/path updated in private process.json. Waiting for user
manual Continue after restart; automated XR A is published (0x1000) but title
ignores it. Do not claim after-fix gameplay or reflection repair yet. If handing
control to user, clearly identify active simulator session; restore normal
Steam payload before headset testing. Reproducible archive gate:
python tools/audit-clip-input-projections.py test-output/material-eye-audit-20261105 --output test-output/crane-smoke-20261105/production-audit.json
PASS all10 identities and complete clip-input paths. No game shader data in Git.
