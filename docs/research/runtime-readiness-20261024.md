# Runtime readiness receipt — 2026-10-24

Task branch: `codex/runtime-camera-readiness`, parent `9fe7944` on
`codex/integration-frame-capture`. No merge into research. Native FOV untouched.

## Present coverage: gap established in source, live observation blocked

`HookDevice` patches Device Present@17 and PresentEx@121. It does **not** install
a SwapChain Present@3 observer. `TryHookSwapChainPresent` is misleadingly named:
its normal implementation patches the engine call site A94052; the diagnostic
build returns before doing that. Therefore its Device COM hooks do not establish
coverage of the engine's known SwapChain dispatch. No new COM slot, entry-point,
EXE or IAT patch was added in this task.

Historical actual-vs-implicit swapchain observations and overlay recursion are
in `verified-engine-present.md`. The actual COM object was different from
GetSwapChain(0), despite the device reporting one swapchain. Installing the old
entry wrapper into slot 3 would recurse: that wrapper resolves live slot 3 on
every call. Neither that change nor another guessed object probe is justified.
This establishes a gap in the current observer, **not** that observing the real
engine through COM is impossible. Actual object discovery/lifetime and a safe
forwarding chain remain necessary before such an observer can be implemented.

The diagnostic now logs Device vtable slots, implicit swapchain identity/slot,
EndScene counts versus Device/Ex/SwapChain Present counts, reset generation,
and witnessed call/return IDs, caller, thread and HRESULT. Enable before launch:

```ini
[Diagnostics]
PresentReadiness=1
MatchedFrameCapture=0
```

Both keys default OFF; MatchedFrameCapture also enables readiness at startup.
No extra hook is installed. EndScene counters measure coverage only; they never
certify a presentation boundary or substitute for one.

## Capture correction

The old sticky `g_stereoPresentBoundarySeen` could accept a later fallback frame
after one historical Present. Capture schema 2 requires a current thread-local
Present witness with matching call serial, logical game-frame counter and reset
epoch; a global reset generation also invalidates captures across reset threads.
Only exact DevicePresent/PresentEx/SwapChainPresent boundary names qualify.
EndScene, trace-only, failed-reset and nested calls cannot create a witness.
These are observed call identities, not invented simulation-frame identities.

The real two-phase schedule is preserved: BeginTrackedFrame at the previous
Present's tail arms the NEXT logical game frame and XR frame. Live eye-0 draw
banks are recorded between those calls. The next matching Present captures RT0,
replays eye 1, and stages the four images and submission metadata. The expected
call serial, device, thread, frame and reset generation must still match; a
fallback EndScene cannot close this window. A pending staged capture cannot be
overwritten by the next XR latch at the closing Present's tail. COMPLETE.json is
published only after the original COM function returns D3D_OK, was actually
forwarded, and still matches the witness. Suppressed desktop Present, missing
original Ex function, failed/occluded/status HRESULTs and changed witnesses cannot
publish it. A crash between staging and return leaves no receipt. The checker
requires matching arm/entry/completion records and stage ordering before Luna runs.
Engine ownership remains explicitly UNKNOWN; a receipt does not identify the
active player camera or prove engine-wide interception.

## Native projection to a concrete CPU rejection path

The existing descriptor/receiver chain remains the starting point. In worker
445E40, the SAME locally composed matrix from 4460BC -> C02F50 is passed at
446148 to 607CD0, then at 44615C to 685050. The context is at the worker's local
ESP+260 (stack addresses adjusted around pushes). At 446175, BC9230 receives
this context and an output list at resource+ B0; its receiver is the pointee
of the retained handle at stage+68. This is a specific resource query, not yet
the main player camera or scene-wide object visibility dispatcher.

* 607CD0 (ECX=context, stack matrix, RET 4) extracts SIX planes from the matrix:
  column 2, column 3 +/- columns 0/1, and column 3 - column 2. It normalizes XYZ
  and negates normalized W, then calls 62DD40 at 607D75, 607E10, 607EAB, 607F47,
  607FE3 and 60807F. 62DD40 (RET 8) appends plane float4s at context+30..38 and
  tags at +150..158, invalidating its derived-state flag.
* 685050 supplements context min/max +10/+20 using D37570 and canonical bounds
  10A14C0; it does not replace the plane vector just constructed.
* BC9230 (ECX=resource, context/output stack args, RET 8) queries a hierarchy at
  resource+3A0 using C0F500. C0F500 tests each non-null child bounds through
  93F2F0 at C0F5A5; false at C0F5AC skips recursive traversal at C0F5C0.
* BC92B2 tests returned candidate bounds (records of stride 40 at resource+388).
  BC92B9 false skips the candidate. BC9381 tests a triangle's min/max built from
  vertex data at resource+60 and index data at +70; BC9388 false skips the index
  append beginning at BC938A. This is a real CPU rejection affecting an index
  output list, not an angular selection inferred from GetFOV calls.
* 93F2F0 (thiscall, bounds pointer, RET 4) rejects an empty context, optionally
  checks XYZ AABB overlap, then calls 48B7F0 on its context+30..34 plane vector.
  48B7F0 (four stack args, caller cleanup) selects the extreme AABB vertex by
  normal sign and returns true for an outside plane; 93F2F0 inverts that result.
  Compare/early-out instructions are 48B890..48B89A. This connects the matrix's
  planes to the caller's actual skip branches, without asserting camera ownership.

Important sample difference: the old B56B depot program contains zeros at
93F2F0. Its bogus ADD disassembly was rejected as evidence. After user-confirmed
AV resolution, the audited original 55C8 EXE was copied privately, hash-checked,
and imported into new Ghidra project `InstalledCamera20261024`. Asserted entries
93F2F0 and 48B7F0 decompile successfully there. Each export ends EVIDENCE COMPLETE.
All eight added instruction-range hashes are reproducible with audit_camera.py;
the other seven ranges also match the depot bytes. No binary/decompilation is
committed. Private exports: d2vr-camera-readiness-query.txt,
d2vr-camera-readiness-reject.txt, d2vr-installed-camera-reject.txt and
d2vr-installed-camera-plane-test.txt in the user's Temp directory.

Lifetime narrowed, not solved: D41D0F increments the retained handle count,
D41D14..31 releases/replaces the previous stage+68 handle, and +6C caches its ID.
8285E0 initializes that resource container to null handle 10A1D60. D41CB9 selects
through owner+4F0 virtual slot 78 and checks type descriptor 10E5568. This proves
retention of that resource; it does not prove ownership/lifetime of the worker's
descriptor[0] camera. Its producer, main camera selection, connection to a
particular VS WVP upload, and scene-wide rejection remain open. No native hook
or widening is justified yet. 007BF850 is not used as culling proof.

## Tests and installation receipt

PASS: matched metadata/checker tests 15/15; frame-boundary suite 2,327,551
assertions, zero failures (50,000 traces, 25,000 timing simulations); audit parser
6/6; Gemini adapter 25/25 including its private trace; Luna self-test; rendered
FOV 1206 rays; clarity/depth suite; production and diagnostic x86 DLL builds.
Normal preprocessed renderer versus parent 9fe7944 is identical after whitespace
normalization: SHA a9bcfd7a4e2f507716bc777e0f5918c16d4155728b039ea5583e8b4dcfa36cd9.
No production INI change. No claim of measured runtime performance equivalence.

User confirmed Bitdefender detection resolved. Installed original audit PASS:
SHA 55c8d634fcbe2996d8397d15d68ea728c999697f1dd40e8779b5afe8bbb4c339,
14 sites and now 18 complete ranges, PE32/I386, timestamp 4F68A875, base 400000,
image size DEC000. Steam RunningAppID was 0 and no game/VR processes were found.
Backups were hash-verified before temporary deployment. Diagnostic DLL SHA was
C5256B59E346E46D2F31C75E5006140F256F6792C28FE35EAD5E82D6C9F56966.

ONE Steam `-applaunch 67370` attempt started Steam PID 33496 but produced no
DarknessII process or new mod log. Thus **Steam game test BLOCKED**, no Present
callback measurements. No repair/verification retries or AV setting changes.
Computer Use native pipe was also unavailable. No desktop screenshots, matched
capture, Luna gameplay comparison or PSVR2 test resulted. A Windows restart may
be the user's next action; its necessity and the launch failure cause are not
established. Do not label this an EXE crash or a successful game launch.

Deployment was restored and SHA-verified before stopping: DLL
9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C;
INI C802439A7AD84E6B4CEA567405B3A5B85BEEA89EB0A00DBF7313FA5437E77436;
preexisting diagnostic INI 586F536AFEF7BA596F6F2D8789A74B6B37F20501B4902FD3EAF4D33C5E1DA9C0.
Local INI and old log unchanged. Private backup/RESTORED receipt:
Temp/d2vr-readiness-20261024-203541. Re-audit after restart before another trial.

## Additional xrsim input: local preflight only

The existing local primary source at vr-mods/references/Dishonored-VR/src/tools/
xrsim declares XR_KHR_D3D11_ENABLE, creates D3D11 sessions/swapchains and implements
the frame lifecycle. Existing manifest xrsim-final-20261009/dvr_xrsim32.json points
to build/src/RelWithDebInfo/dvr_xrsim32.dll: PE machine 014C, 1,007,104 bytes,
SHA 5d86a93b7d27e2e6ecc0dc7501aad492468d4c554cea119965d2e4b9870d1ecc.
Its old log identifies SingularityVR, not Darkness2VR validation.

This is a plausible separate deterministic OpenXR test runtime; no Darkness II
compatibility or execution is claimed. No runtime registry/environment changes
were made. Darkness2VR currently has no XrRuntimeJson INI override. A parent
process XR_RUNTIME_JSON cannot be assumed to propagate through an already running
Steam client (the existing xrsim-launch.ps1 documents that limitation). A future
process-local override needs a separate opt-in design before a Steam xrsim trial.
Simulation will not prove real engine Present coverage or physical PSVR2 fusion.
