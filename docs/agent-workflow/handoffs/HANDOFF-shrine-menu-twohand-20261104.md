# MAIN shrine light, menu anchor and two-hand aim

Branch codex/shrine-menu-twohand-20261104, worktree .main-camera-shake.
Baseline main9d21de1c, accepted DLL9476A8D5, diagnostic117230AC, main INI22862511.
Previous menu/wheel-only equip user accepted and pushed main.

Scope: identify moving light at purple talent shrine; anchor VR Settings to
actual native menu quad; let left support hand steer the actual weapon/shot
and improve supported accuracy. Preserve independent dual wield, tentacles,
menus, resolution and exact rollback. MAIN only runtime/deployment.

Evidence before edits: ShowEntry currently returns once the space matches,
so entry pose never follows RecordMenuSurface. ProtectedWeapon uses only the
right aim pose for slot0. Existing shotgun support only poses the left wrist.
CFBBA0 final CFBD60->C47570 boundary is already digest-pinned. Native pellet
generator B27AA0 has RET14h and accepts out, matrix, cone, seed, fraction;
cone<=0 copies matrix row+20. Nonzero cone offsets row+20 using row0/+10,
action+114/+118, then normalizes. No native field offsets will be guessed.

Plan: shared support-pose math for model/ray/wrist, private owned shot-consumer
call override for supported cone reduction, exact menu quad anchoring. Capture
shrine render evidence in simulator before applying any world-light patch.
Tests must distinguish fixtures/simulator from headset acceptance.

Implemented candidate
- VR Settings AnchorMenu follows cinema quads and, separately, the gameplay
  sprite-menu plane derived from the selected native projection and 1.55 UI
  scale. Rendering and laser use the same surface; invalid poses are rejected.
- Generic saved two-handed owner replaces shotgun-only support. Left squeeze
  latches near the fore-end (50cm engage, 65/35% hysteresis); right origin and
  roll retained, both hands determine model/wrist/actual-shot direction. No
  dual-wield or held-demon target is admitted. Native recoil animation unchanged.
- Private digest-pinned C47570 clone rebinds only C47743->B27AA0 to narrow a
  supported native spread cone by half. Native seed/fraction/pellets/damage/
  collision preserved. Original native EXE bytes remain unchanged.
- Shrine captures identified light-map specular consumers: VS293F456B 948B,
  SHA ce46fc92b8eb1c017b9da0a97fb5feca3ad8a457f178e52604ea64d73e574121;
  PSBA3D8E61/CA08BDC4 1704B. Their v2 tangent eye direction consumes VS c8.
  Admit full digests, then existing Correct() validates native c8 against raw
  WVP camera center before replacing it with actual device WVP center. Preserve
  time component and exact draw-scoped restoration. Other shader families and
  the default-off cube-reflection experiment remain unchanged. No blanket
  change to light positions, particle transforms or shadow matrices.

Validation (2026-11-04, simulator, not headset acceptance)
- Normal and DiagnosticCapture builds PASS.
- Two-hand math 45 checks PASS; consumer CFG/header reproduction PASS.
- Protected weapon actual-source fixture:125256 checks +3200 native-body
  fixture invocations PASS. This fixture does not admit the new generic owner;
  runtime shotgun support and native fire are checked separately below.
- Settings input/laser4564, renderer995, host routing50, submission7215 PASS;
  actual reflection draw-scope tests PASS including both admitted families,
  mismatched pairs, unknown banks, driver failures and exact restoration.
- Simulator49196: pause entry follows head translation/yaw/pitch; controller
  laser opens settings. Source screenshots settings-open/menu-still/menu-moved
  remain private in test-output/shrine-menu-twohand-20261104.
- Simulator shotgun: support=1, move left hand with right fixed, fire and
  release. Native log records cone12->6; support hand/model captures retained.
- Capture2120 frame5565:42 light-map draws (21 per eye), max distance between
  actual WVP center and c8=0.000005393 game units. Both native PS variants
  admitted. This proves corrected eye coordinates, NOT complete removal of
  every reported light artifact or headset comfort/FPS benefit.
- Matched capture staged_result incomplete with isolated-eye-output blit
  witness; do not call the entire capture PASS. Local saved draw/bank identity
  and numeric c8 comparisons above remain independently inspectable.
- Capturing heavy stages can trigger transient render-heuristic cinema and
  rebase head reference. Do not use post-capture screenshots as a paired
  geometric comparison. Raw shader/geometry payloads stay private/untracked.

Delivery: build/shrine-menu-twohand-20261104/Install.ps1 is the frozen combined
candidate, with exact rollback to accepted9476A8D5 DLL +117230AC diagnostic.
Main INI/SS125, EXE, loader and accepted DLC marker repair must match baseline.
Headset acceptance and whether the user's specific extra shrine light is fully
resolved remain NOT RUN. Do not merge this new candidate before user testing.

Frozen source4dca29fe, DiagnosticCapture DLL
85CDD777E1279A750188008877E024BC5911D2A725AEEDC7741A0CC405B5D811.
Profile gate and frozen installer VerifyOnly PASS. Steam restored to accepted
9476A8D5; all four original save/config hashes match saves.json. Simulator
process49196 closed after identity check. No new candidate installed for HMD.
