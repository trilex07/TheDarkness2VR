# Menu/wheel/input candidate installation

MAIN candidate, 2026-11-04. Source `1b63e9bf88861871bbf9a1f716d50335706ba340`.
Task branch `codex/menu-wheel-input-20261104`; accepted prior changes are on main.

Installed together with the game closed:

- d3d9.dll: `407E78B42EFCD63A3A45018D0C3EC91DBE58665562851AE9E7FA1AD8168B6EE9`
- Diagnostic profile: `117230AC717C2A5FE7C1CA82BF6A693B2A056855D4B47F18CCA941325462C076`
- DLC H.Script.cache: `692FC3CD8D850E72CB3C168F9A8B559E0FDF0F04FC6E904D4D17C8EFD5FC986B`
- DLC H.Script.toc: `0B9A879C0EB3C88E693747397C3D5D08A861FD3352CC5B51C28A3738DF1CFB3E`

PASS frozen payload verification, install, exact four-file restore, reinstall
and installed readback. Simulator OFF in final installed profile. EXE, loader,
main INI (including SS125), Polish dialogue/text cache identities preserved.
All four original user save/config hashes match their pre-test backups.

Private package `build/menu-wheel-input-20261104/Install.ps1`: `-VerifyOnly`
checks all payloads and installation; `-Restore` restores the exact baseline
DLL/profile and DLC pair. No owned game assets committed or published.

Simulator/actual-source results and limits are recorded in the per-task handoff.
Visual talent-tree verification and headset acceptance remain NOT RUN.
