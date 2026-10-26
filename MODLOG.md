# Task branch engineering log

## 2026-10-24 — Offline native camera/FOV follow-up

On `codex/native-camera-fov`, based on research `cb6638f`, verified different
Lua receiver type globals for GetFOV and the base-FOV setter. Identified the
FCOS-to-dot-product candidate filter, perspective-shaped constructors
`00A31FF0` / `00DF3B50`, and eight-corner constructor `00DF53E0` with Ghidra
and static instruction evidence. Active player projection and CPU visibility
producer/consumer are still open; no native hook was implemented or deployed.

Preserved the private Ghidra project outside Git. Added the existing read-only
PE audit and its six passing parser tests to version control, guarded explicit
Ghidra entry assertions with original bytes, and prepared a disabled observation
manifest. The depot sample is rejected by the audit's installed-SHA policy even
though all six instruction sites match. Steam/PSVR2 testing waits for the user
to resolve the antivirus detection and safely restore the installation.

Detailed evidence and limits: [native camera audit](docs/research/native-camera-fov-audit.md).


## MAIN 2026-10-24 - clean offline integration and opt-in matched capture

Approved picks: 744e637, accd9b2, f978db8, 5f59fe1, ddaf4af, in that order.
New dedicated integration worktree; original untracked MODLOG preserved.
Added compile/runtime-off one-frame RT0/final-eye capture with draw banks,
metadata, completeness receipts and checked Luna analysis. No Gemini adapter.
No production INI change or deployment. Ordinary preprocessed production source
matches the integrated baseline. Native camera/CPU visibility ownership remains
open; static worker/dispatcher/bounds evidence and reproducible range hashes
added. Restored installed EXE passes identity/14-site/10-range audit, but AV
resolution and native-patch-free Present observation still block runtime gates.
Details and validation: docs/research/matched-frame-capture.md.


## MAIN 2026-10-24 - Gemini diagnostic toolchain accepted

Integrated ae8de22, 0f9085f, 85be172, 120b9e0 once, preserving previous series.
Acceptance fixes: opaque op=7 viewport becomes UNKNOWN; op=36 clears both shader
constant banks, provenance and pending context so rebind/partial upload cannot
resurrect pre-Apply data. Eight new tests plus original 17 pass, including the
private real trace. Corrected L8=50/D16=80 against local SDK. Existing capture
10/10, PE parser 6/6, Luna self-test PASS. Production src/, INI and build unchanged
versus prior integration; original untracked MODLOG untouched. No Steam work.
Adapter remains diagnostic evidence only; no active-player-camera proof claimed.

## MAIN 2026-10-25 - Actual additional swapchain, observer rejected

Proved 00964C40 CreateAdditionalSwapChain(slot13) -> QI SwapChain9Ex ->
owner+20 -> A94055 Present; owner+1C is the base fallback. One XR-off/stereo-off
Steam trial of a per-instance public-length shadow failed before acquisition:
system D3D9 calls private device slot156, beyond Device9Ex's 134 SDK methods.
Removed the unsafe runtime prototype, restored installed DLL/INIs with matching
hashes, and stopped further live testing. Added reproducible EXE/module byte
audit and six fail-closed regression tests. Actual Present callbacks remain zero.
Details: docs/research/actual-swapchain-observer-20261025.md. No FOV work or merge.

## MAIN 2026-10-25 - Isolated engine Present candidate, approval pending

Prepared OFF-by-default EnginePresentCallsite diagnostic exception using the
historical A94052 CALL dispatch, exact SHA/PE/site checks, first-CreateDevice-only
installation, recursion rejection and current capture witness/return gate.
No COM shadow. Offline x86 instruction/ABI/rollback tests, frame suite, 15 capture
and 13 audit tests pass; production preprocessing unchanged. No deployment or
game launch. Explicit user approval required for the in-memory EXE change.
Details: docs/research/diagnostic-engine-present-candidate.md.

## MAIN 2026-10-25 - Approved mono Present trial, baseline restored

Tested exact 7b5ee3d candidate, XR/capture off. Actual swapchain 0F8A0FE0 differs
from implicit 0F781760; caller A94057, thread26060. Last sample 44000 entries =
forwards = returns, recursive0/fault0; all 47 sampled HRESULTs D3D_OK, no logged
exceptions. Per-call HRESULT history is not available in this candidate.
Intro/title observed; gameplay and graceful menu shutdown not validated.
Stopped test PID and restored DLL/both INIs; EXE/local INI hashes unchanged.
No next VR test. Receipt: docs/research/engine-present-mono-20261025.md.
Recorded separate prior-PSVR2 issues VR-001 (P0 right tentacles), VR-002 (yellow
edges), VR-003 (detail loss); prepared one-frame capture plan without execution.

## MAIN 2026-10-25 - Complete HRESULT accounting, next candidate offline

Every diagnostic Present return now contributes to ok/status/error counters;
nonzero HRESULTs log immediately and negative results latch capture rejection.
Added readiness checker with seven tests; x86 ABI/rollback and 15 capture tests
pass. Previous sampled log correctly remains insufficient for full HR proof.
Built isolated candidate; no deployment or new game/VR run. Concrete bounded
test: docs/research/matched-capture-next-candidate.md. P0 remains right tentacles.

## MAIN 2026-10-25 - Approved staged trial: mono PASS, capture incomplete

Exact 4fcef42 mono checker PASS covers 44000 forwarded/returned/OK, zero
status/error/recursion. One approved stereo request frame3822 failed missing
same-eye projection witness: producer is live-only, absent in replay path.
Partial source-eye0 is 1280x855 and already contains yellow edge; includes pause
overlay. 121 draws/eye do not identify right-tentacle geometry. No final images,
COMPLETE or valid Luna analysis. No retry; test exited, DLL/INIs restored with
hashes, EXE/local INI unchanged. Next fix is diagnostic witness provenance only.
Receipt: docs/research/staged-capture-20261025.md. All three issues remain open.

## MAIN 2026-10-25 - Complete capture, shared-transfer race correction

Fixed successful PS-upload witness for both eyes; completed unpaused frame5408
with user in HMD. Source RTs differ but both final eyes contain right-source pixels.
Found internal left StretchRect entering replay before reader fence; moved record
shutdown before internal transfers. Exact tentacle shaders/mesh slices show native
right rest pose above/behind camera; no invented pose correction. Rejected unused
c20 hypothesis before activation. Added separate OFF-by-default73EBA724 previous
projection correction for proven false motion vectors. Production/diagnostic builds,
Present witness,15 capture+7 readiness+3 transfer tests and336 math checks PASS.
Fixed renderer not yet run: another Steam/PSVR2 game active. DLL/INIs restored with
hash verification; no native FOV/EXE disk write, no merge. VR-001 remains OPEN.
Receipt: docs/research/vr001-render-fixes-20261025.md.

### 2026-10-25: production-resolution capture candidate

Added explicit OFF-by-default diagnostic ProductionResolution to exercise the existing production GetClientRect sizing path without enabling native FOV/gameplay hooks. Restrict spoofing to the actual device window and require the USER32 GetProcAddress result plus C9646E call fingerprint. Audit includes that exact call. Capture config records requested size and hook state. Initial real Steam startup: depth target 3076x3140 (previous diagnostic 1280x855), matched color/eye capture pending user-ready. No native FOV or sharpening change. Both x86 builds pass; camera audit 7 tests and matched capture 17 tests pass. Luna viewport check now accepts valid full-XY world depth range [0.05,1]; rejects partial XY and invalid depth.

### 2026-10-25: bounded yellow-pass localization prepared

Existing matched capture gains opt-in CapturePassStages=1: left-eye RT0 snapshots only at target transitions, at most 8 attempts/256 MiB, plus 32 bounded shader bytecodes, all tied to frame/draw/RT/hash metadata. Uses readbacks only; no draw/culling/FOV changes. Auxiliary files are private and SHA-256 verified by the capture checker; failed optional images are explicitly marked and cannot serve as image evidence. Ordinary production and capture-OFF paths do no additional GPU work. Both builds pass; matched-capture regression suite 20 tests passes. Candidate saved privately, not deployed into the running high-resolution test.


### 2026-10-26: persistent-log A/B, runtime wait removal and crash blocker

Actual Steam attempts A4665f73/B7eb665a used unchanged 2048x2091 stereo INIs.
Both exited after C0000005 at 00E129EA; separate approval obtained for B after A.
No third launch. Common 8s stationary prefix: 80 Log/CloseHandle/filter waits
(168.229ms) -> zero; p95 16.407 -> 13.737ms, worst 72.738 -> 130.105ms.
B has no combat rows, different shader warmness/runtime period: not a sustained
combat or 90FPS acceptance. Worst B frame spends 97.865ms in shader creation.
Ghidra confirms native lazy per-variant caches at owner+0x60 PS / +0x50 VS; no
unsafe hash-only shader reuse added. Retain 7eb665a; fix collector full baseline
duration; final DLL/INI/local INI/EXE rollback hashes verified.
Receipt: docs/research/perf-persistent-log-ab-20261026.md and aggregate JSON.

### 2026-10-26: exact native-crash reproduction and image-write isolation

Read-only pinned-EXE regression reproduces BOTH recorded E129EA source pointers.
A's FC2664 class-vtable replacement alone yields A1561184; B's C63009 aim patch
alone yields D966AF14. Native code hashes image regions and uses the transformed
hash to decode source addresses. Original inputs produce valid F36A00/755C00.
This establishes a mod hook-installation regression for those faults, rather
than an unexplained native object-lifetime failure. No native validator bypass.

Implemented OFF-by-default diagnostic BlockNativeImagePatches write admission,
including the two proven sites and other image writers. It refuses VR/capture:
only mono isolation is currently supported because verified Present still needs
an EXE write. It is NOT a full-VR crash fix or HMD acceptance. Image-preserving
interception/ownership remains the exact blocker; moving detours is not proven
safe. Tests: 7 exact-pointer/model regressions; x86 image-guard/Present rollback;
existing Present ABI,1329 tracked-gate checks,2327551 boundary assertions;
capture31/readiness8/audit7; both DLL builds PASS. Steam/INI/EXE baseline hashes
unchanged. No launch, deploy, merge or parameter changes.
Receipt: docs/research/crash-relative-pointer-20261026.md.
SIDE shader branch0f9c5f5 separately demonstrates a loading-prewarm safety
blocker: failed native shader creation can erase the bytecode slot. No unsafe
prewarm implementation or claimed gameplay improvement.

### 2026-10-26: image-preserving Present and protected stereo candidate

Implemented opt-in external implementation-entry trampolines for verified
CreateAdditionalSwapChain -> QI9Ex -> Present. Real COM objects/private tables,
arguments, refs and HRESULTs stay native; no swapchain table copy or EXE write.
Per-entry immutable originals prevent the previous saved-original overlay cycle.
Reset/fault/thread changes close runtime XR admission. Protected SubmitMode4
opens only inside the native presentation path after a successful warm-up;
EndScene is never a substitute. Image-write guard remains latched, including
IAT/data/vtables; initial native gameplay/input/FOV/sizing hooks remain disabled.

Both x86 builds PASS. Actual native fixture/trampoline tests PASS for base/Ex,
private methods, existing overlay JMP chain, 1000 release/recreation cycles,
Reset/statuses, recursion/error/thread rejection and EXE text preservation.
Guard/Present ABI/frame-boundary/capture regressions PASS. Installed EXE/DLL/INIs
match preserved rollback hashes. Persistent log fix retained, SIDE prewarm
findings preserved separately. Concrete private DLL/merged profile prepared;
Steam/actual overlay/PSVR2 NOT RUN. No deployment, launch or automatic merge.
Receipt: docs/research/image-preserving-present-20261026.md.

### 2026-10-26: protected Present live PASS; gameplay stereo NO-GO

Exact7ed10a6 package tested twice, second launch explicitly requested by user.
Real additional-swapchain Present forwarded/returned D3D_OK for cumulative
44000/33000 samples, recursion/errors/statuses/fault0; startup Reset passed.
Overlay loaded/coexisted; visible overlay UI not verified. No CRC-pointer fault
recorded; retry was explicitly terminated after failed normal UI close.
Both eyes generated, but focused gameplay submitted layers0; PSVR2 user saw black.
Root cause in protected profile: inherited NativeStereoHeadGeometry1 with
HeadGeometry0 deterministically invalidates renderTrackingValid in gameplay.
Added only NativeStereoHeadGeometry0 to protected override,2 profile regressions
PASS. Correction OFFLINE ONLY; old private tested package preserved unchanged.
Both sessions restored original DLL/four INIs and EXE hashes, rollback PASS.
No further launch, native patch, quality change or automatic merge.
Receipt: docs/research/image-preserving-present-live-20261026.md.

### 2026-10-26: corrected protected profile sends gameplay XR layers

One user-authorized Steam test of d722e95 with a newly merged INI package and
unchanged EE0F3C DLL. Both geometry flags0; image guard/external observer1.
Real Present last sample22000 D3D_OK, recursion/errors/statuses/fault0.
FOCUSED gameplay:25/25 sampled frames layers1; continuous sampled span50.10s;
XR_SUCCEEDED layer cadence89.6..90.0/s (not simulation FPS), no xrEndFrame error.
User confirmed VR image visible. Controller/head tracking remain intentionally
OFF; low source resolution and full stereo comfort acceptance remain open.
No CRC access-violation recurrence;3 nonfatal first-chance C++ exceptions retained
in private evidence; normal user exit0. No broad stability claim.
DLL/all four INIs restored, all hashes and original EXE verified. No second
launch, renderer/DLL changes, unsafe hooks, quality changes or automatic merge.
Receipt: docs/research/image-preserving-xr-layer-pass-20261026.md.

### 2026-10-26: protected replay resource retention candidate (SIDE)

Implemented default-OFF UniqueReplayResourceRefs: one owned COM reference per
exact interface pointer per frame, no draw/state elision or cross-frame cache.
Native shader prewarm remains blocked; no guessed native owner access added.
x86 production/diagnostic builds and 10,000-frame-per-mode ownership regression
PASS; synthetic pairs20,480,000 ->640,000. No measured game-time claim.
Protected profile2/2 and existing1,206-ray projection tests PASS. No Steam,
deployment, native-image write, FOV/resolution/sync change. MAIN reviews/integrates.
Receipt: docs/research/protected-replay-resource-refs-20261026.md.

### 2026-10-26: MAIN protected VR integration continues

From accepted 85eea69, integrated protected external XInput (0dafa48), bounded
source sizing (c2e2304), frame-local reference retention (e657960), and corrected
head/eye transform plus inverse projection (77f6d65/33eb527). f8d00ab isolates
experiments in composed profiles. All new features remain OFF by default.
Both DLL builds and integrated ABI/geometry/lifetime/profile checks pass offline.
First input-only package is frozen; no Steam or installation changes.
Full native tracking/visibility, human GPU-bone mapping, native pickup ownership
and shader-loading lifetime retain specific blockers, not success claims.
Receipt: docs/research/protected-vr-integration-20261026.md.

Protected stock gestures integrated from SIDE 24a3482 as cca749a: objective, hip
reload and right planar stroke; no native targeting/throw bridge. User requested
head plus gestures with input in one candidate. Composed profile now explicitly
owns reconstruction and bypasses reading old Shift+digit choice via new default-ON
RestoreSavedLighting option, without changing the saved lighting file.
Combined profile still keeps source-quality and performance experiments OFF.
Native visibility/hand mapping blockers remain; no physical success claimed.

### 2026-10-26: combined protected controls/head/gestures live checkpoint

4281f29 ran once with user authorization. HMD feedback: movement/gestures work,
head/stereo question answered okay apart from blur; outdoors20-30msCPU persists.
Present sampled37000 successful forwards/returns, no recursion/errors;25 sampled
layers1 contexts share eye/frameIDs;7 right strokes/2 reloads. Exit0, no recorded
CRC AV;3 first-chance C++ events. All5 DLL/INI backups restored/hash-verified,
EXE unchanged. Actual source1920x1071;90FPS and clarity NOT MET.
Next work is narrow COM-retention A/B with missing protected CPU-stage reporting
corrected, not another general profiler. Receipt:
docs/research/protected-vr-combined-runtime-20261026.md.
