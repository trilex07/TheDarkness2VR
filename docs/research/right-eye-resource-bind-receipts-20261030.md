# Right-eye resource binding correction

Baseline: fetched MAIN integration `969b3b987d1dc0b43092327d6a52e3df42b74f2c`
(`codex/main-render-cpu-next`); the remote `main` is an older baseline.
Branch: `codex/permanent-right-eye-flicker`.

## Defect and correction

The texture and depth-surface hooks recorded a bind before calling D3D9.
If the live bind failed, the left eye retained resource A. Replay could later
accept the recorded resource B and draw with different texture/depth state.
The hooks also queried and potentially latched B as hardware depth despite
the unsuccessful native bind. Shader binding hooks already commit only after
success; these two resource hooks did not follow that contract.

Forward the original bind exactly once, return immediately on failure, then
record/retain the accepted resource and update depth observations. Preserve
the original HRESULT, null unbinds and all successful binding order. A failed
bind no longer produces a replay command or retains a rejected resource.
This preserves the existing live binding rather than dropping an otherwise
correct pair or suppressing geometry. No INI or stereo mathematics change.

## Evidence

The new fixture extracts both complete hooks from `src/d3d9.cpp`, and the
texture/depth replay cases from `src/stereo_replay.inl`. D3D9 calls are modeled;
the existing real frame reference ledger and replay binding cache are used.

Before correction:

`powershell -ExecutionPolicy Bypass -File test/build_live_resource_binding.ps1 -SourceRef 969b3b98`

fails check 33 at the left/right binding equality assertion. The rejected
texture bind leaves the left eye on A while actual replay binds B.
After correction the same fixture passes 1,577 checks across 240 combinations
of recording, filtering, texture/depth, sampler slots, null binds and results.
It also checks a failed replacement after a successful same-frame bind,
unchanged INTZ identity, release of rejected resources before replay, exact
native-call count/HRESULT and balanced references at teardown.

Existing F12 pair `test-output/f12_021_eye{0,1}.png`, frame trace
`run_43052_frame_003_trace.txt` and earlier correction notes were reviewed.
The trace contains full identified inverse-projection writes, not a captured
partial PS matrix defect, and the recent runtime counters show SB=0.
These artifacts do not contain a failing texture/depth bind receipt. This is
a proven source defect, **not proof that it explains all reported flicker**.
The existing WVP/STP, PS history, UP, VB/IB and native camera corrections remain.

## Validation

- Production and DiagnosticCapture x86 DLL builds: PASS.
- New actual-source binding regression: PASS (baseline FAIL above).
- Live record status: PASS, 5,688 checks.
- Replay surface receipts: PASS, 128 streams / 896 calls.
- Replay binding cache: PASS, 4,000 matching observed draw states.
- Projection/history math: PASS, 145,248 checks.
- Live StateBlock/shader synchronization: PASS, 296,163 checks.
- Pixel metadata publication/binding: PASS.
- Actual replay constant loop: PASS in both flavors, 58,510 checks each.
- Color history: PASS, partial/UI and rejected segments.
- UP geometry and frame resource ownership: PASS.
- Left aim: PASS, 461 checks; protected hands: PASS, 2,233 checks;
  tentacle head scope: PASS, 26,629 checks.
- Game/PSVR2 stereo, depth, HUD, hands, tentacles and FOV: NOT RUN; MAIN gate.

Production DLL SHA-256:
`2F93665086C1692E227A2050855E90609139A58938CEE5A56E82ADE654A59D54`.
Diagnostic DLL SHA-256:
`1C15726827E72A12539A27978AF776EC4BBD1D42C5962B200517357F53D2BD9C`.

No additional driver calls, GPU synchronization, allocations, locks or readbacks
are introduced. Success adds an HRESULT check; failures avoid recording and
diagnostic queries. Runtime CPU/GPU/FPS impact is unmeasured. No game launch,
Steam deployment, merge or physical acceptance was performed. Permanent removal
of the observed flicker remains unconfirmed until MAIN's consolidated PSVR2 test.

## MAIN integration validation

Integrated onto exact MAIN 969b3b98 on codex/main-right-eye-resource-bind.
Cherry-pick fc6012d3 changes only the two native resource hooks in production.
No configuration, FOV, constant transformation, buffer versioning, haptics,
hands, tentacles, menu, font, input, OpenXR or Steam files were changed.

Original pointers come from PatchVTable slots 39/65 and are also supplied directly
to the replay original-call table. The hooks do not hold the recorder lock across
the original call. RecEmit locks only after the accepted synchronous bind returns.
Accepted native bindings own their resources before recorder reference retention;
INTZ ownership transfer and balanced GetContainer references are unchanged.
Reset cleanup and replay resource ledgers are unchanged. This preserves the
existing render-thread contract; it does not claim arbitrary concurrent device
calls are serialized by these hooks.

The MAIN fixture additionally verifies nested native callback completion ordering
and five successive accepted binds with binding filtering OFF and ON. Actual
hooks and replay cases PASS 1649 checks; baseline 969b3b98 fails at check 33.
Live hooks 5688, surface receipts 128 streams/896 calls, binding cache, projection
history 145248, StateBlock synchronization 296163, PS metadata concurrency,
resource lifetimes, color history 2000 segments, UP geometry 4403, left aim 461,
tracked hands 2233, tentacles 26629, buffer lifecycle/disabled bypass, PS payload
576, constant batching 80190, and actual float loops 58510 checks per build flavor
PASS. Both x86 DLL builds and 44 actual-binary profile guard checks PASS.
An initial UP generated fixture in shared TEMP did not match this checkout and
failed compilation; regenerated under a private TEMP, the unchanged test PASSed.

Standard DLL SHA256:
62078C5359D84BF1AC3793ACA48CA1A9716E547CB914FD55686B0CA1C0B3AF1D
Protected-native DLL SHA256 (Steam candidate):
1E882174FE6089FA1F698E4DB4F45D0E937E7D712DEF0CA27A9266302D181C68

Private staged packet: %LOCALAPPDATA%/Darkness2VR/packages/right-eye-resource-bind-fc6012d3.
Its deployment helper validates the recorded game/INI/DLL preimage and refuses
an active game. Rollback retains the previous CPU candidate and all current INIs.
No installation or launch while the user tests the Polish font correction.
Next consolidated PSVR2 gate: flicker at rest/slow head motion, both eyes/depth,
peripheral rooms, shadows/HUD, tracked hands/tentacles, and outdoor frame pacing
and CPU time together. Runtime visuals and FPS/CPU impact remain NOT RUN.
