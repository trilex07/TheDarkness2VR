# Handoff: targeted storage cleanup

Date: 2026-11-05. Task ID: storage-cleanup-20261105. Role: MAIN.
Branch: codex/storage-cleanup-20261105. Checkout: .main-camera-shake.
User authorized unnecessary project/test/cache cleanup for Darkness2VR and
Singularity in vr-mods. No gameplay/source changes or integration requested.
Installed Darkness candidate E8C8015E remains unchanged; acceptance pending.

Status: Done for the verified disposable subset. No broad folder deletion.
Source/active tools, current captures (Nov4 onward), learned shader caches,
Ghidra/native research, audio models/assets, deployment packages, recoveries,
Steam installations, saves, NVIDIA and other apps were excluded.

Private evidence: test-output/cleanup-20261105/{inventory.json,plan.json,
protected-before.json,receipt.json,preserved-reports.zip,
preserved-reports-index.json,prepare-plan.py,cleanup.ps1}.

836 explicit untracked files removed:445 compilation intermediates,239 old
Singularity top-level BMP/log captures,111 old temporary Darkness BMPs,
33 old main test BMP/video captures,5 old raw ETW traces,3 downloaded browser
symbol-cache files. All older than Nov4. No tool installation removed without
proof that its consumers no longer need it. No user-owned video library touched.

Logical removed26,004,849,852 bytes (24.219GiB). Free space delta since inventory
24,934,305,792 bytes (23.222GiB); after30,353,899,520 bytes (28.269GiB).
These are separate measures: filesystem sharing and concurrent activity may
make free-space changes differ from logical sizes. No hardlink saving claimed.

Preserved187 reports/logs in verified ZIP, SHA256
ac4340c244cc6c9eff43bb54ec7c5e2683a281228dfdf99a2aead79f3584a1e7.
All836 planned paths independently confirmed absent;23 protected hashes match,
covering Steam DLL/config/assets, current candidate/rollback and4 Darkness saves.
Tracked git status remained clean for both project checkouts before this note.

Initial preflight rejected an incorrect UTC comparison before deletion; corrected.
Deletion succeeded but first receipt serialization failed because Windows
PowerShell Measure-Object cannot measure these hashtable keys. Independently
rebuilt receipt from exact planned-path absence and protected hashes; the helper
now sums bytes explicitly. No failed deletions or changed protected files.

Inventory was bounded, not a complete disk audit: skipped reparse points and
.git;1659 long-path/missing-path errors mainly in installer fixtures. No deletion
was selected from those fixtures. Kept large active dubbing outputs and release
packages; original inventories available for a separately justified next pass.
