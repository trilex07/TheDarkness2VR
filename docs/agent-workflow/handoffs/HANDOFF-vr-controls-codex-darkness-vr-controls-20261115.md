# Handoff: English native menu and VR Controls

- **Date**: 2026-11-15
- **Role**: SIDE (offline)
- **Branch**: `codex/darkness-vr-controls-20261115`
- **Worktree**: `C:/Users/Trilex/Darkness2VR-controls-20261115`
- **Baseline**: `331d5b41`, public catalog 1.1.8; separate from the accepted Steam binary.

## Goal and status

English game menu when Polish text is unchecked. Add an English VR Controls
guide beside VR Settings in the game's title and pause menus. Staged for review.

## Changes

- Two buttons share the existing compositor strip. VR Controls opens a
  read-only guide with PS VR2 Sense and Meta Quest Touch names, including the
  objective hold, release during a throw, weapons, movement and menu controls.
- Existing neutral-release lease and pointer/surface rules apply. No gameplay
  bindings or native menu hooks changed.
- Companion installer branch `codex/darkness-native-menu-20261115` rebuilds the
  English native menu recipe and rejects an accidentally Polish EN source.

## Evidence

- Reviewed BASELINE, FAILED-APPROACHES and public audit handoff.
- DiagnosticCapture x86 DLL: `BB8E9570C955366ACDCDE4D8DFB0AC8AC56538B28A6D303836DD2778BA1AE79A`.
- Settings input (4,589), GDI renderer (5,192), host routing (50) and submission
  (7,215) checks passed. Real GDI metrics verified fit on each guide page.
- Catalog-pinned old EN menu decoded to `Kontynuuj` / `Opcje` / `Wstecz`.
  New recipe decoded through the installer to `Continue` / `Options` / `Back`.
  Polish module and source pins are unchanged. No Steam files were used.
- The unrelated private language helper fixture requires a working Windows
  PowerShell Get-FileHash in its child process; this host did not provide it.

## Next steps

MAIN: try EN and PL installs/updates and verify the title menu and pause in
headset before publication. Validate actual PS VR2 Sense and Quest Touch input.
The staged installer archives are in `build/installer-candidate`, with an
unsigned catalog. Follow the installer packaging note before publication.
