# Task branch engineering log

## 2026-10-24 â€” Offline native camera/FOV follow-up

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

### 2026-10-26: replay-local constant packets (offline, default OFF)

Resolved 1722/1732 B-combat ntdll memset direct callers to WOW64 service
marshalling; six UNKNOWN, four WOW64 callbacks. Native hot PCs mapped read-only
on D46B identity. Implemented adjacent same-stage float4 replay batching after
original per-upload stereo transformation, with caps/barrier/failure/capture
guards. No EXE writes or Steam changes. Actual game performance NOT VERIFIED.
Evidence/tests: docs/research/outdoor-cpu-packets-20261026.md.

### 2026-10-26: remove unused PS staging and bound actual light payload

Continued after packet commit: unchanged replay PS uploads borrow the owned
arena; only the verified light correction materializes a1..224-vector copy.
Removes unused copies and the old >224-vector temporary overflow. Actual-case
regression288 cases PASS; production/diagnostic x86 builds PASS. Added the
requested optional read-only cap fields to existing SHA/PE auditor (not invoked
on a process). Native938-function aggregate shows distributed work; no native
code/heap mutation or Steam access. Performance/HMD acceptance still NOT RUN.

### 2026-10-26 — Continue outdoor CPU: actual upload receipts + isolated float-only window

- Count original replay VS/PS float API entries separately from logical records,
  failures/vectors/exceptions, with existing StereoPerf/QPC windows; production
  omits diagnostics. No new profiler/per-frame log or Steam run.
- Implement ReplayConstantWindow=0: independent VS/PS dirty runs within float-only
  sequences, flush on EVERY other command, transform first, no state reuse across
  barriers, no invented gap values, invalid-call fallback and exact-once failure.
  Adjacent A/B remains ONLY ReplayConstantBatch0/1; window/copy/async remain0.
- Actual-source x86 regression432072 records/62893 snapshots per build; prior
  protected Present/head/input/gesture/lighting/recorder tests PASS. Both DLL
  configurations built/frozen. Old trace counts4471->3362 adjacent,2523 window;
  no current-outdoor CPU/FPS saving measured. 6.44ms wall deficit unresolved.
- Preserved installed DLL/INIs, image guard, stereo, tracking and input; no merge.
  Next live A/B needs user authorization. Details in
  docs/research/outdoor-cpu-packets-20261026.md.

### 2026-10-26 — Adjacent batching A: preserve partial evidence, stop B, rollback
- Authorized exact f86d0dc A completed10s stationary/20s outdoor combat; user
  saw VR and closed normally after end cues. Real Present13000 successful
  forwards/returns, no recursion/image-write/fault receipt or native AV.
- A combat18.53983ms mean,p9522.22909,p9924.56798; GPU Busy3.03491ms. Actual
  recorded VS/PS setter counts equal original calls; no failed upload/exception.
  B was NOT started: ETW lost3828807 events and collector postprocessing hit a
  null regex input. Zero-loss PM timings survive; full CPU A/B is unavailable.
- Existing collector fixed to retain loss/UNKNOWN and process handle;23 actual
  helper checks,4 PM regressions,owned exit0/37 checks PASS. No renderer/DLL or
  effective gameplay change. Batch/Window remain default OFF; no FPS saving claim.
- All5 original DLL/INIs restored with SHA matches; EXE unchanged. Raw evidence
  private; detailed bounded verdict in existing outdoor-cpu-packets research doc.

## 2026-10-27 - Level preloading and residency gate

Dedicated SIDE base63b04ea. Reused existing ETL; the130.105 ms shader hitch has
97.865 ms shader/driver blocked time and no disk reads in its bounded disk report.
Eight-second window has5 F.Texture.cache reads/320 KiB/1.107 ms disk service.
Pinned captured x86 EXE is already LAA. Ghidra B61190 retains receiver+0x0C and
requests via manager010A1CBC virtual+28; loading/async/owner contracts unresolved.
No helper, residency override or shader warmup admitted. See
`docs/research/level-preloading-residency-20261027.md` and companion evidence JSON.

## 2026-10-27 - Actual default-OFF startup shader preparation prototype

Added same-device startup create/release pass before native device return, with a
bounded private SM3 package and no native getter/owner access. No persistent shader
references or async D3D9 calls. Full production/diagnostic x86 builds and core,
package, stereo transfer and actual replay regressions PASS.19 available private
shaders packaged;84 trace hashes missing. Driver/game benefit still unverified;
see tools/shader-startup-warmup/README.md. No deployment or Steam/game launch.

## 2026-10-27 - Fixed-field native material capture

The a57e131 user-run log confirms image admission and 1,491,991 local-buffer
copies / 9,277,452 inspected uploads, zero copy faults. These are cumulative
receipts across mixed package ON/OFF gameplay, not a matched FPS comparison.
Verified native builders 571670/AD3380 keep current owner in EBX for their whole
body (one entry assignment, epilogue restore). Added an x86 API gate to carry
incoming EBX/EBP/return-slot by value, and per-call recognition of nine exact
fixed-field submission sites. No native pointer persists and no native code or
table changes. Default-OFF NativeOwnedMaterialCopy requires the existing image
admission; loaded full producer bodies are fingerprinted. Detailed ownership,
regressions and runtime acceptance gate are in
docs/research/native-owned-material-copy-20261027.md.

## 2026-10-27 - Common owner matrix and exact native source target

Extended fresh owned material capture to the common VS571E68/PSAD3B78 callback,
only for current block+330 and exactly four vectors. Added SourceQuality=4 to
request explicit native RenderWidth/Height through the protected external sizing
observer. Actual x86 gate4073 and both external source-size fixtures PASS.
No native code/table changes or cached matrices. Runtime FPS and actual upstream
2676x2728 resolution remain unverified. Integration and evidence boundaries:
docs/research/native-matrix-and-exact-source-20261027.md.

## 2026-10-27 - Remove duplicate recorder payload initialization (offline)

RecPush's byte-vector resize used to zero every appended byte immediately before
Copy overwrote that complete span. Use a byte-only arena allocator that starts
byte lifetimes without value initialization. All existing capture locks, payload
offsets, size caps and copy error policies remain. Copy must complete before
RecEmit publishes any corresponding record; every failure path still explicitly
fills the readable prefix/zero tail. No permissions, source pointers or payload
contents are cached. Audit found no other appender to g_recData.

Actual RecPush x86 regression2085 PASS, including dirty capacity reused across
frames, null/guard/noaccess/partial sources and 8MiB growth. Production and
diagnostic builds PASS. This removes a proven duplicate write, but its game
frame-time saving is unmeasured. The retained outdoor ntdll memset samples have
UNKNOWN ancestry and cannot be attributed to this resize or claimed as savings.
Installed candidate81cc723 remains frozen for its pending user test; this follow-up
is not deployed.

## 2026-10-27 - Single-lock payload and command publication (offline)

RecEmitData previously entered/exited the same recorder lock for RecPush and
again for RecEmit. Split private locked helpers from standalone locking wrappers
and hold one lock across payload append and command publication. Standalone
RecPush/RecEmit consumers retain locking and resource-reference bookkeeping.
Recheck active/overflow under lock and refuse full streams before source copying.
This reduces two lock acquisitions/releases to one per data command; it neither
removes synchronization nor parallelizes the native device API.

Production bodies are extracted into the x86 fixture, including the actual RecOp
and Rec layouts. Four concurrent producers emit 4000 uniquely tagged 64-byte
payloads; all command offsets/content and identities match. Inactive/full stream
checks confirm no data append or bad-source read and released lock. Combined
copy/guard/frame-reuse regression6087 PASS; native gate4073 PASS; frame resource
refs10000 frames/mode PASS. Production and diagnostic builds PASS. Runtime saving
is NOT MEASURED; installed81cc723 remains unchanged for the pending test.

## 2026-10-27 - Existing trace hitch analysis and shader-hook I/O removal

Reused retained ad8db6a zero-loss ETL and exact symbols, without a new capture.
Bounded timestamp-joined extraction has zero stack timestamp mismatches. The
111.2981ms PresentMon interval at CPUStartQPCTime110257791.2832 contains native
BC78E0 scene teardown (NxReleaseControllerManager, actor releases, scene release)
and texture destruction. It must not establish a gameplay physics bottleneck.

The completed transition's 85.4547ms interval at CPUStartQPCTime110226615.4232
contains 28 main-thread sampled weights. Stacks prove native pixel-shader creation,
shader validation, a NVIDIA CreatePixelShader wait, and Hook_CreatePixelShader ->
GetPrivateProfileIntA -> INI file open. Samples do not establish exact blocked
duration or attribute the whole interval to one cause. Private bounded outputs:
analysis-derived/hitch-{111,85}ms-chains.json and hitch-native-contracts.txt in
material-window-capture-20261027T140923Z/trace/20261027T141205Z-c4e8bb85.

Read ShadowScreenDepth once in LoadConfig, before any device hook/thread uses it.
Remove the unconditional 32KiB replacement-array zero fill: replacement bytes are
used only after a complete fread extent and shader-header/end validation. Make
automatic PS file dumps opt-in via startup Diagnostics/ShaderDebugDump=1, default
OFF. InverseProjection/ViewExtents reflection and map updates remain unconditional.
Explicit ShadowScreenDepth replacement-file reading remains allowed when enabled.
These diagnostic settings now require restart. No shader-result cache or native
owner change. Production/diagnostic x86 builds and actual D3D9 REF preload tests
(ownership, misses/HRESULT, bytecode identity, concurrent requests, Reset) PASS.
This removes proven hook I/O; FPS gain and remaining driver wait are unmeasured.

## 2026-10-27 - Keep bounded startup shader inventory through menu delays

Existing ad8db6a trace log records startup569 shaders,92 periodic reports (at
least182s between first and last), ending537 requests/170 hits/575 learned.
Hits reach170 at report58 and remain there. Source inspection found unconditional
120-second expiry followed by releasing one unused precreated object per frame.
This is a definite premature-inventory-expiry behavior; historical counters do
not prove that every miss or the entire85ms creation interval came from expiry.
Missing archive identities and repeated already-consumed requests still exist.

Removed wall-clock expiry from Tick. The existing1024-entry/code-size bounds,
single-device/full-bytecode match and one-use ownership transfer are preserved.
Unused precreated COM objects now retain their allocation until consumption or
pool teardown rather than being destroyed while the player waits in a menu.
This trades longer bounded object residency for avoiding late startup misses;
it does not enable shared shader-object caching, async D3D9 creation or reuse
after native owner destruction. Failed Reset still disables serving.
Existing periodic receipt now adds pending inventory count.

Actual x86 D3D9 REF regression advances Tick by one hour for1024 ticks, proves
three pending objects remain and later requests use them without original API
creation. Existing consumed-repeat, exact bytes/device/stage,16-thread ownership,
guard behavior, failed/successful Reset and persistence tests PASS. Production
and diagnostic DLL builds PASS. Gameplay/FPS saving remains NOT VERIFIED.
