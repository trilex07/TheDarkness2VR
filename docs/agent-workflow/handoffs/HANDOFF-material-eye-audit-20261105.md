# Handoff: wider material eye audit

Date: 2026-11-05. Task ID: material-eye-audit-20261105. Role: MAIN.
Branch: codex/material-eye-audit-20261105.
Worktree: C:/Users/Trilex/Darkness2VR/.main-camera-shake.
Baseline: accepted main beace8e3, Steam DLL7DD55BA4. Inherits unaccepted
crane/ammo candidate cf4c3869 (89540B8A). Integration pending.
Context loaded: BASELINE, FAILED-APPROACHES, workflow, prior crane evidence.

Goal: review similar head-dependent specular/reflection consumers beyond crane.
Status: offline tests and bounded gameplay draw verification complete; headset acceptance pending.
Changes: identity/pair allowlist, metadata flags, guarded ReflectionEyeScope,
reproducible private-archive audit, source fixtures and freeze evidence argument.
Evidence: docs/research/material-eye-audit-20261105.md; private
 test-output/material-eye-audit-20261105/audit.json.
1427 collected shader identities,203 manifests ->13 VS/176 PS/184 observed pairs.
No game bytecode committed. No full-game or headset coverage claim.

Tests PASS: extracted production scope both eyes/rejection/restore, actual-source
pixel metadata concurrency/lifecycle, protected-native x86 build, diff check.
New build CEAFF3BC. Simulator starts, main menu renders. Navigation does not
react to CUA keyboard/mouse or simulator controls despite delivery acks.
User asked asynchronously to select Continue if available. Do not count acks
as successful navigation or certify gameplay from menu-only startup.

Recovery: own process identity in private process.json; current four user save
backups and hashes in saves.json; simulator lab exact rollback payload.json.
Before ending, close only own identity-checked PID, restore simulator payload
and four fresh files, verify exact hashes. Final recovery state below.

Next: gameplay sample for admitted new families, eye-center ownership and
CPU cost check; physical acceptance; only then integrate accepted candidate.

Recovery PASS: own emulator closed, exact accepted Steam DLL7DD55BA4 and
diagnostic117230AC restored; protected files unchanged; four fresh saves/configs
restored and SHA-256 checked. No game running. Private recovery.json records it.
Candidate remains experimental until gameplay/headset verification.


Follow-up 2026-11-05: user selected Continue, gameplay tested in retry lab
material-eye-retry-20261105T142053. Found and fixed ReflectionEyeScope being
constructed before TentacleHeadScope in all four draw wrappers. Final matrix
must be set before c8 correction; original nested restore order preserved.
Final DLL E8C8015E58045B6F8EA2FE744E142540FEB478608F40D3FB508B8DB4B484C72B.
790 draw witnesses/53 pairs/8 VS, both eyes,3 head poses, <=0.000138057
model-unit residual versus existing0.001 ownership tolerance. Translated
attached-tentacle residual improved0.25 -> <=0.000002781. Remaining5 VS are
offline only. No full-game, performance or physical headset completion claim.
Exact accepted Steam and four fresh saves restored; no game running.
See detailed research evidence for serials, numerical limits and package.
