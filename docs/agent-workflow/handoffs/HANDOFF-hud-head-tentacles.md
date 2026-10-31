# Handoff: native HUD hiding and head-attached tentacles

Date: 2026-10-31
Agent role: MAIN (authorized installation; user launches manually)
Branch: codex/hud-head-tentacles
Worktree: C:/Users/Trilex/.codex/worktrees/hud-head-tentacles/Darkness2VR
Baseline: user-selected main cb795d58; installed predecessor DLL verified 6CA7A1F5.
Status: offline validation passed, deployment pending.

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
