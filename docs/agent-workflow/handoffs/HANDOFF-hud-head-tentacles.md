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

Deployment completed 2026-10-31 21:30 Europe/Warsaw after explicit user go-ahead.
Installed source2f027f05, DLL469F7B92 (full SHA above). Package:
C:/Users/Trilex/AppData/Local/Darkness2VR/packages/matched-header-20261031T213047682
Full rollback in rollback/Restore.ps1. Game closed; exact EXE/DLL checks PASS;
all four profiles and four caches preserved byte-for-byte. No game launch.
Next: user launches and loads the affected corridor, then authorizes/readies
one matched capture with the corrected recorder. No lighting fix claimed.

## Runtime request102: camera timing fixed, full validator still rejects
User loaded affected corridor; one request on PID35604/renderTID21232 captured
XRframe5396 at C:/Users/Trilex/AppData/Local/Temp/Darkness2VR-matched-35604-102-5396.
Exact installed DLL469F7B92/stock EXE verified. Original diagnostic bytes restored;
backup C:/Users/Trilex/AppData/Local/Darkness2VR/captures/world-light-20261031T213541736.
Both source/final eyes, 508 draws/eye, 8 stages/255504480 budgeted bytes and
original Present success receipt exist. Header now matches camera_frame5396;
no previous native-camera timing rejection. Source-eye0 inspected: correct
station corridor, contextual hint remains visible. No lighting acceptance.

Full check_matched_capture.py rejects `invalid hand skin upload identity`.
Four hand_skin_upload rows occur AFTER source-eye0 (manifest rows2711/2712,
5288/5289, zero-based), caller6449DD1E, null EXE RVA, before_draw508; these are
128-vector renderer restoration uploads misclassified as live-eye skin data.
BeginMatchedSkinUpload only excludes replayEye>=0; after replay/restore it can
be -1 again. Keep validator strict; do not modify raw records or call full
package valid. No automatic repeat of this request. Source correction should
exclude post-source/internal restore uploads with actual-source regression.

Stage01 ends draw191, VS68EEE79C, PS C4D1FB10 (shadow reconstruction).
Private shader-preload archive contains bytecode with matching FNV32;
shader C4D1FB10 SHA256=7fb0cc4d119b3f93336344f4d44bb3907d79465371fa3ef5d67c4062734bb751.
This is archive provenance, not a direct same-draw shader bytecode witness.
Offline D3DDisassemble confirms VS c0 WVP/c4 ScreenTextureProjection and
PS c27 InverseProjection/c16 ShadowMapTextureProjection, depth sampler0,
shadow sampler4. The stage shader budget32 exhausted before these shaders.
Existing draw banks contain only VS constants, not effective PS matrices.
Need second same-position head pose for comparison; async readiness requested.
No further DLL/config render changes made. Capture readbacks are not FPS evidence.

## Request103 and bounded recorder completion
Second user-ready request: PID35604, serial103, XRframe24450, folder
C:/Users/Trilex/AppData/Local/Temp/Darkness2VR-matched-35604-103-24450.
Both eyes and8 stages saved; original Present completed,514 draws/eye. Full
validator still rejects post-source hand upload identity; do not relabel as PASS.
Original diagnostic bytes restored; backup:
C:/Users/Trilex/AppData/Local/Darkness2VR/captures/world-light-20261031T213914823.
Source image has pause overlay; world behind remains rendered. Eye pose change
from request102:30.2938 degrees and0.113966m eye displacement. Not a pure yaw
or pixel-registered controlled lighting acceptance. No additional automatic shots.

Recorder source now excludes hand uploads once source-eye0 was captured (both
begin/end); the existing validator's rejection remains strict. Lighting draws
read actual PS c0..223 at consumption into manifest lighting_ps_constants with
shader/draw/frame identity, reflected inverse/extents register and HRESULT status.
Validator checks finite896-value bank, valid registers, matching shader, unique
draw and ordering before source. Eight of32 shader slots reserved for lighting.
All changes diagnostic-only after Current/capture guards; no rendering mutation.
Tests: actual-source39 checks PASS; validator32 tests (including finite bank,
identity/register/failure/ordering cases) PASS; protected x86 build PASS.
New candidate DLL SHA256:
C306DA8AA8008514F1672BFC27628A799358A3C4AEE6B002241F2141A98A5C20.
NOT INSTALLED; current Steam DLL remains469F7B92. Deployment script updated to
require that exact predecessor and latest nine-target rollback template.
World-lighting root cause and fix remain OPEN. Required missing evidence is
effective same-draw PS reconstruction/lighting matrices, not another VS-only shot.
