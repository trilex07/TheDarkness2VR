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

## Follow-up: world-lighting bug and FPS correction
User confirms moving light/shadow patches on walls/floor. OPEN; need two loaded
same-scene head poses with matched render evidence. No shadow fix claimed.
Latest log confirms HUD contract/hidden=1 and both-eye mesh-scoped tentacle
receipts. It does not prove complete HUD visual coverage (screenshot retains
contextual hint text). User has not explicitly accepted HUD behavior.

Installed correction (2026-10-31 21:16 Europe/Warsaw): AdaptiveFPS=OFF in both
primary INIs only. DLL155D6A42 unchanged; no Steam/game launch. Nine-target
verified rollback/evidence package:
C:/Users/Trilex/AppData/Local/Darkness2VR/packages/hud-fps-off-20261031T211614129
Retained before-d3d9.log proves AUTO/HALF at90Hz and42-45 app/layer cadence.
The missing key inherited AUTO from main, inadvertently preserved in our first
candidate. Source/repo defaults fixed to OFF, startup reporting explicit.
Build PASS; policy3253 and protected head117021 PASS; adaptive integration
execution blocked by sharing lock after bounded retry (not PASS).
See research note follow-up for exact frame/wait evidence and next capture.
User launches the game; wait for loaded-scene readiness before any capture.
No remote push attempted; prior destination approval remains pending.

## Matched-capture recorder correction (offline)
One user-authorized request serial101, PID35892, XRframe6108 failed at
C:/Users/Trilex/AppData/Local/Temp/Darkness2VR-matched-35892-101-6108.
Manifest rejects selected-camera changes after its header; no COMPLETE/images.
This is invalid evidence for lighting. Do not retry this binary automatically.
The temporary diagnostic request was restored byte-for-byte. Game kept running.

Root cause: Begin writes the eye configuration before the same-frame native
selected-camera callback refreshes it. Header now freezes at first consumption
after same-frame native confirmation. Present/frame/reset/device/config checks
remain mandatory; later camera updates still reject. Previously failed capture
cannot write a header or consume data. No render or shadow behavior changed.
Offline: actual-source x86 gate34 checks PASS; capture validator31 tests PASS;
protected diagnostic DLL build PASS. New binary SHA256:
469F7B92F316C34DBE05C4DDBE4707C4122C568D4EE64E99478494B2B90D03A7.
tools/deploy-matched-header.ps1 stages only this DLL and backs up all nine
targets; preserves existing INIs/caches exactly. Deployment requires game closed.
Runtime capture acceptance and world-lighting fix remain OPEN.
