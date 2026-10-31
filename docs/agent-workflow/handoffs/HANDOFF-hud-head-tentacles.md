# Handoff: native HUD hiding and head-attached tentacles

Date: 2026-10-31
Agent role: MAIN (authorized installation; user launches manually)
Branch: codex/hud-head-tentacles
Worktree: C:/Users/Trilex/.codex/worktrees/hud-head-tentacles/Darkness2VR
Baseline: user-selected main cb795d58; installed predecessor DLL verified 6CA7A1F5.
Status: deployed candidate; manual HMD acceptance pending.

Scope: native gameplay HUD visibility, existing tentacle head attachment enabled.
Read: supplied trilex_vr_mod.md skill, VR wizard/RE skills, bootstrap, BASELINE,
ACCEPTED_BASELINE, WORKFLOW, FAILED-APPROACHES, adaptive-half-rate HUD follow-up,
protected-weapon held-target correction. No subagents, no game launches.

Code: src/native_hud.h/.inl, src/native_aim.inl callback, src/d3d9.cpp config,
Darkness2VR.ini candidate keys; profile flavor gate, host tests and EXE pin test.
Evidence/plan/results/manual checks: docs/research/native-hud-head-tentacles-20261031.md.

Tests: standard+protected x86 builds PASS; HUD467, tentacles26629, six EXE pin
ranges plus stock EXE SHA256 PASS. HMD behavior NOT RUN. User explicitly approved
installation in chat and will launch the game themselves. Preserve native hold
refusal: moving a held target with the head is outside this candidate.

## Deployment receipt
Implementation commit: 5e70b9db (base cb795d58).
Protected-native DLL: 155D6A427E4D53AD5EBEAE206E17C4A7F755ABBD4FEBC2C8D5629FAFF48D1AE1.
Package: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/hud-head-tentacles-20261031T210354632
Rollback: package/rollback/Restore.ps1 (all nine predeployment targets SHA-verified).
Only DLL plus Render/HideNativeHUD=1 and HeadAttachedTentacles=1 in Steam/local
primary INIs changed. Existing controls, rendering settings, diagnostic profile,
lighting selection, caches and EXE preserved. Installed key read-back PASS.
Game closed before copy; no agent game/Steam launch. Standard flavor rejection
and protected flavor admission PASS. No visual acceptance claimed.

The first deployment attempt triggered an over-strict preserved-file comparison
because the candidate used forward slashes and the manifest used backslashes.
Full rollback executed and verified. Canonical absolute path comparison fixed;
second installation passed. This was a deployment-script error, not a game test.

Next: user runs the five-item manual checklist in the research note. In logs,
look for [NativeHUD] contract=admitted/hidden=1 and [HeadAttachedTentacles]
mesh-scoped receipts in both eyes. A missing owner/type means no HUD write;
use the guard evidence instead of broadening shader suppression or guessing.

Remote push was rejected by automatic approval review because destination-specific
permission to export repository contents was not explicit. Local commits retained;
no remote push or PR creation completed.
