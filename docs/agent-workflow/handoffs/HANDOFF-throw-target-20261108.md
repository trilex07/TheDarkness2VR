# Handoff: gaze/controller throw target assistance

**Date**: 2026-11-08  
**Role**: MAIN  
**Branch**: codex/throw-target-20261108  
**Worktree**: C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107  
**Base**: 3eed105c (installed gaze-stability source lineage)  
**Integration**: pending; no merge or physical acceptance

## Inspected state

Actual installed DLL SHA256 D72F8E1CBED5CE41D7F90B5775C6CC89A55F39CA0F69933D3A01021C4CA498EE.
It matches the 2026-11-07 frozen gaze candidate, source03e7b38d/0c0511e1,
documented by3eed105c. Git fetch confirms origin/main9515f875, 2026-11-05.
The root checkout side/texture-hires-pipeline-light2dae9c34 and BASELINE.md
are older and must not replace this installed build.

## Request and change

Prefer filtered interaction gaze, fall back to left controller aim at release;
choose the NPC closest in angle to that ray. The stroke still triggers release.
Input/ThrowTargetAssist defaults ON and makes the aim contribution100%, including
when older installed profiles specify ThrowAimAssist20/GazeThrowAssist85.
Setting ThrowTargetAssist=0 retains the configurable blended direction.

Private release clone overrides only the native query and trajectory observation
in addition to the existing velocity comparison. Query CB0040 is SHA-pinned,
CFG-audited and copied to owned RX storage with124 relocated direct calls.
The controller/eye origin is frozen in reference-local metres, validated and
scaled to native world units at query consumption. The camera getter admits two exact native helper return addresses and one
private query return address; the scoped callback checks render thread and camera.
No EXE/static table or original camera matrix is written.

The native7BE900 eligibility filter always executes and consumes its two by-value
references. Scope adds a D2NpcAvatar type requirement; special BraggProjectile
early-return is suppressed. Primary/fallback native candidates are ranked by
angular error at the native sort boundary, retaining native reference swaps,
collision checks and target lifetime. Cone12degrees; native range is retained.
Script-forced targets returned before the native candidate sort retain native
priority. D2NpcAvatar includes NPC subclasses; hostility remains the stock
eligibility policy, not a newly invented team-field offset.

A target selection alone does not authorize preserving stock physics. The exact
E0BCA0 call adapter observes successful AL first; otherwise physics uses the
gaze/controller ray. Native speed/spin/W and the one-shot owner/entity gates remain.
If selector preparation fails, the existing direct throw remains available.

## Evidence

- test/native_throw_target_contract.py: full pinned EXE/query hashes, closed CFG,
  124 calls,22 instruction/layout/ABI/class contracts and degrees conversion PASS.
- test/build_protected_left_throw.ps1:384 checks PASS; actual source bodies,
  selection/fallback, NPC/native-filter refusal, exception scope recovery,
  native trajectory-success requirement and EDI/ECX/ESI/stack adapter checks.
- test/build_native_aim_getter.ps1:151017 checks PASS including scoped callback,
  registers/flags/x87/SSE/LastError/reentry and unchanged source image.
- test/build_native_view_code_clone.ps1: PASS including multiple overrides,
  duplicate rejection, RW-to-RX sealing and existing10000-call regression.
- Eye tracking51 checks and protected left aim444 checks PASS.
- Normal build.ps1 debug link fails known MSPDB140 mismatch; release link needs
  /DEBUG:NONE, /Z7 and a fresh output/import-library directory. An initial release
  link to the old root import library failed access5; fresh output succeeded.
- REA batch decompilation exceeded the client450s timeout. No repeated analysis;
  this implementation's evidence is local Capstone/pefile contracts, not missing
  decompiler output. REA must be closed after its provider drains.

## Remaining runtime verification

Offline tests do not establish real hostile NPC acquisition, visibility policy,
native external callee compatibility after relocation, actual object flight or
physical PSVR2 behavior. Do not claim a gameplay fix solely from these tests.
The root workflow forbids injected input. A gameplay trial needs a user-driven
scene with an ordinary throwable and two visible enemies at different angles;
test gaze, >160ms gaze loss/controller fallback, off-axis/occluded NPCs, then
pause/recenter and throw ownership transitions. Keep saves/config backed up.

tools/prepare-throw-target-candidate.ps1 freezes a DLL-only package after commit,
checks the exact D72F baseline and preserves profiles/caches/loader/EXE.
Package Install.ps1 supports -VerifyOnly and -Restore for the exact baseline.
No installer release, Git main merge or accepted-baseline update is authorized
by this handoff alone.

## Frozen result and approval blockers

Implementation commit0449d252228643d699f6a761745b74be1ab5ae52.
Final protected-native x86 release build PASS, DLL SHA256
F98BFEFDD4A4405AE6E534BE8DF2932880F2888DF3A096D64DC87B8171B919D1.
Final source body tests384 checks PASS. Frozen package
build/throw-target-steam-20261108/Install.ps1 -VerifyOnly PASS: one DLL payload,
exact rollback, baseline identity and eight protected installed files checked.
Normal launch/profile gate PASS. Source preparation role was MAIN in this
handoff, but automatic approval review classified the session as SIDE.

Automatic review rejected these operations before process execution:
- git push -u origin codex/throw-target-20261108: potentially private source
  exported to an unverified destination without explicit payload authorization.
  Destination from git config: https://github.com/trilex07/thedarkness2vr.git.
- Package Install.ps1 (then -VerifyOnly): changing the live Steam DLL is
  MAIN-only; reviewer treated this session as SIDE/offline preparation.

Do not bypass or retry without explicit user authorization (or genuinely new
authorization evidence). User approval is requested for these exact operations.
No installation or runtime test occurred. Installed DLL remains D72F8E1C;
candidate F98BFEFD is local and unaccepted. No merge, release or push occurred.
REA close_binary completed after its timed-out analysis drained.
