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

## 2026-10-27 - Retained hitch: shader and native query critical waits

Bounded CSwitch/ReadyThread export of the same zero-loss ETL, no new capture:
mainTID20940 in16185351..16270807us has24365us scheduled and61091us off CPU,
partitioning85456us. Scheduled residency includes any DPC/ISR overhead; it is
not pure exclusive execution. PresentMon CPUBusy84.9786ms was not scheduler RUN.
Matched switch pairs and resumed thread waiting stacks identify16 wait intervals
/32660us in CreatePixelShader,5 intervals/24451us in CreateQuery,3980us other.
Intervals are NOT API invocation counts. Largest query wait10186us and shader
wait7606us have only10us/9us ready delay. This identifies a particular transition
hitch, not steady outdoor90Hz. Observer overhead is not subtracted.

Installed-image Ghidra SHA verified:9FBDD0 loads device from argument, passes
current object's field+4 as output, pushes type9, calls vtable+1D8 (slot118),
returns at9FBDE5. SDK type9 is OCCLUSION. This newly established high-impact native
producer is outside the closed resource cycle. No query correction installed yet.

Next implementation boundary: bounded precreated unused occlusion-query inventory,
one transfer per object to the exact caller/device/type. Preserve Issue/GetData,
null-output support probes, native errors and exhausted-inventory fallback.
Queries are lost on device loss, so unused inventory must be released before
Reset/ResetEx and recreated only after success, unlike shader retention.
Reference: https://learn.microsoft.com/en-us/windows/win32/direct3d9/asynchronous-notification
Actual D3D9 ownership, Issue/GetData and Reset regression is required before use.
Private evidence in existing capture analysis-derived: hitch-85ms-scheduler.json,
hitch-85ms-scheduler-raw.txt and hitch-query-site-asm.txt. Native EXE unchanged.

## 2026-10-27 - One-use native occlusion-query startup inventory

Implemented default-OFF Render/QueryPreloadCount (0..256). Protected diagnostic
installation requires the exact installed EXE SHA, base400000 and all24 bytes
of native constructor9FBDD0. Only caller9FBDE5, matching device, OCCLUSION type
and non-null output qualify. The API gate preserves the original stdcall ABI.
Unknown callers/types/devices and null-output support probes forward unchanged.

Create at most the requested number of never-issued queries at startup, before
native device handoff. Each object transfers once, with its original creation
HRESULT and no extra retained reference. Exhaustion/native creation failure
retains the original API; consumed queries never enter the inventory again.
No Issue/GetData interception or substituted visibility values. No async creation.
The2-second soft creation budget is checked between driver calls, not a promise
to interrupt an individual blocking driver operation.

Release unused objects before Reset/ResetEx, disable serving on failed Reset,
refill synchronously only after successful Reset. Check TestCooperativeLevel for
classic devices / CheckDeviceState for Ex before transfer. Preserve the fixed
device/window identity; unavailable device-state checks fall back to original.
Counters expose pending inventory, qualified requests and hits. This feature is
startup/Reset scoped and does not toggle with L3+R3's replay CPU package.

Actual x86 HAL/Ex fixture gives2016 visible pixels for both pooled and native
query, also after Reset. Type/caller/device/probe bypass, pool exhaustion, native
failure HRESULT, failed/successful Reset and ResetEx PASS. REF lifecycle fixture
also PASS, but its zero result does not establish raster visibility. Both DLL
builds PASS. Installed-image/runtime admission and gameplay FPS remain NOT RUN.
This targets measured query-creation stalls, not a claimed24.45ms average saving.

## 2026-10-27: install native sizing before window construction

User b6ef258 test: minimum reported72FPS, CPU ON, both eyes correct, visibly low
resolution. Saved runtime log confirms source1920x1071 despite2676x2728 XR eyes
and requested Exact sizing. This is NOT native2676x2728 performance acceptance.
Shader preload served444/512 requests; query preload served128/392; native copy
faults remained0. This uncontrolled test does not isolate individual FPS gains.

Verified installed-image native lifecycle: C27160 calls CF2C30 (loads D3D9 and
invokes Direct3DCreate9/Ex) BEFORE A0B540 creates its HWND and calls C96440.
C96440 reads GetClientRect at C9646E and stores manager dimensions. Installing
the sizing observer in HookDevice misses this initial call. Move installation
to both factory export entries, outside DllMain and before native window creation.
Keep exact SHA/PE/call/import admission. Before CreateDevice supplies the HWND,
only the exact native sizing caller may bind a successful, process-owned HWND.
After that, existing exact-window filtering and Reset window updates remain.
Other callers, invalid HWNDs, failures and null outputs retain native behavior.
Add an explicit unconfirmed-size log when RT0 is seen without a sizing request.

x86 real external-trampoline tests PASS for Quality, Exact and pre-window Exact
lifecycle, including invalid-window/wrong-caller/null rejection, BOOL/last-error,
one original forward and unchanged test EXE text. Production and diagnostic
builds PASS. Game sizing, upstream dependencies and FPS require runtime testing.

## 2026-10-27: additional startup inventory for measured creation misses

b6ef258 user-test log ends with shader requests512/hits444 and native occlusion
query requests392/hits128/pending0. These counters prove remaining forwarded
creation requests, not that every shader miss has the same cause or cost.
Retained ETW separately proves critical-path shader/query driver waits.

Add StartupShaderCopies=1(default)/2: create independently owned references for
each archived exact bytecode, at most2048 objects across1024 identities. Cover
all known identities once before preparing second copies within the unchanged
10s soft startup budget. Each result/reference transfers once; no live refill,
object recycling, native cache changes or asynchronous D3D9 calls. Archive format
and legacy single-copy behavior stay unchanged. Increase optional query limit
to512 so the observed392 requests can fit if startup/Reset creation succeeds;
keep2s soft budget, per-call health checks, caller gate and native fallback.

x86 REF and HAL/Ex tests PASS: two identical shader requests served without
native creation, independent Release lifetime and exact bytecode; query512
transfers, native exhaustion fallback and clamp; HAL visibility2016 pixels
before/after Reset. Source sizing lifecycle regression and both builds PASS.
Installed candidate also includes213bfd3 pre-window resolution correction.
Extra inventory targets measured creation misses; sustained90FPS at native
2676x2728/eye remains unverified and is not inferred from these offline tests.

## 2026-10-27: recorder unlock uses release publication

Retained outdoor ETW includes RecLockGuard/RecEmit execution. Replace the
recorder's volatile/interlocked lock with lock-free atomic LONG acquire and
release ordering, retaining Sleep(0) contention policy and the same critical
sections. Unlock now needs a release store, rather than an x86 interlocked
read-modify-write/full fence on every command. Manual replay acquisition and
release use the same atomic protocol. No command elision or concurrent access
to the protected arena is admitted. Actual-source x86 recorder regression
6087 checks (including concurrent uniquely tagged payloads) and both builds
PASS. No runtime millisecond or FPS saving claimed.

## 2026-10-28: reuse frame-start ALL state-block object

Add default-OFF ReuseStateSnapshots. The frame-start snapshot retains its
D3DSBT_ALL object and refreshes values via Capture every frame instead of
CreateStateBlock. Render targets/depth references are released and reacquired
each time. Cache identity is the exact device; changed device or failed Capture
releases it and falls back to creation. Both Reset paths still use full
ReleaseRecording before native Reset, including releasing the cached block.
The live-end snapshot remains independently created/released as before.
This removes one repeated state-block creation and caps query per frame after
initialization, not any native engine state changes or draw operations.

Actual-source DeviceSnapshot x86 tests on REF and HAL PASS across1000 changing
render-state/VS-constant/viewport captures compared with legacy creation,
same-object retention, surface-reference clearing and successful Reset followed
by recreation. Both builds PASS. Retained ETW motivates reducing per-frame D3D
work but establishes no exclusive saving for this change. Gameplay/FPS NOT RUN.

## 2026-10-28: reuse the independent live-end snapshot as well

Extend ReuseStateSnapshots to the second ALL state block, captured after the
live left-eye render. It remains a separate object from frame-start and is
recaptured before replay every frame. Release explicit RT/depth references after
restoration; both Reset paths fully release its cached block before native Reset.
The bounded wrapper is process-lifetime allocated, so no COM release occurs from
its static destructor. Allocation failure uses the existing local snapshot.
Backup failure fully releases it. Default-OFF behavior remains local creation.

Actual-source tests now alternate independent start/end constant values across
1000 cycles and prove neither capture overwrites the other; legacy state,
viewport, reference cleanup and Reset tests still PASS on x86 REF/HAL. Both
builds PASS. Combined mode avoids two repeated ALL block creations per pair;
it does not skip fresh capture or claim a measured runtime/FPS saving.

## 2026-10-28: integrated replay-window regression refreshed

Audited MoreFloatBeforeBarrier: invoked only after float records, so intervening
material commands are scanned once, not quadratically. No algorithm change
justified. Updated actual-source regression extraction boundary to the current
atomic release-store unlock. Production and diagnostic tests each PASS96 streams,
576096 records and70090 barrier snapshots, including batching/capture bypass,
transformed bits, caps, failures and SEH no-retry behavior. Runtime DLL unchanged
from2331082; this test-only commit is not a new performance candidate.

## 2026-10-28: stop disabled readback retaining native render targets

Hook_SetRenderTarget previously called GetContainer and retained up to 400
textures even with LightReadback OFF, without releasing them on Reset. The
retained bdab1cd runtime log contains 11 RTCont acquisitions and no enabled
LightRB window. This establishes unnecessary lifetime extension, not its VRAM
size or its contribution to the reported flicker/FPS.

Collection now requires an active diagnostic budget. Enabled diagnostics own
both surface and texture identities, and release them after the final armed
window or before either Reset entry point. Holding the surface prevents wrapper
address reuse from colliding with a stale diagnostic key. No diagnostic queries
or resource references are added to the normal OFF path.

Actual-source x86 tests cover OFF1000, duplicate and failed acquisition, the
400-entry cap, expiration and Reset/recollection. Real x86 D3D9 REF tests pass
20 OFF/ON cycles: a retained DEFAULT-pool RT prevents classic Reset; releasing
the diagnostic references permits Reset. This does not establish equivalent
ResetEx behavior. Both production and diagnostic DLL builds pass.

Peripheral investigation also traced selected render-camera ownership through
the view object's +A8 reference to the renderer's +3F4 camera reference. The
view class constructor 6D3B10 calls CreateIoCompletionPort before returning,
with ESI holding this, and its virtual +F0 method 6DBE80 calls 985D80. These are
static leads for access without an EXE patch, not authorization to retain an
unowned pointer or install a new native hook. Private SHA-pinned Ghidra exports
remain outside Git. Native frustum coverage is still unresolved.

## 2026-10-28: exact live projection at right-eye replay

VS float records now retain the live camera Px/Py in two previously unused record
words. Replay uses those coefficients, including an explicit unknown-camera state,
instead of the inverse projection observed at the end of the live frame. This
prevents a later PS camera upload from changing the transform of earlier geometry
in only the right eye. Record size, payload ownership and allocation count stay
unchanged. Separately fix the median estimator insertion cursor: its saturated
sample count previously pinned seven startup samples indefinitely.

Actual-source x86 regression passes known/unknown camera changes, warmup, 100
camera transitions, outliers and invalid inputs. Seven transfer-order tests and
production/diagnostic x86 builds pass. No headset flicker or FPS improvement is
claimed. Native visibility still precedes GPU-only FOV expansion; this correction
does not establish a fix for native edge culling.

## 2026-10-28: external ESI view boundary candidate

Diagnostic startup Render/NativeViewBoundary=1 observes the exact native final
QPC return A6677B from view update virtual+B0. ESI is captured before C++ entry.
The prior 6DBE80/+F0 lead belongs to another render path: retained gameplay ETW
instead contains 888972, following view+A8 -> 99F1B0 -> 888720 -> 985D80.
SHA-pinned instruction/vtable/import checks gate the external Win32 trampoline.
EXE code, IAT and static native vtables are unchanged. Default0 disables it;
restart with0 or restore the package to roll back.

The callback validates view+A8 -> renderer+3F4 -> camera, then compares its
inverse projection with a subsequent live primary-world PS upload in the same
frame. Numeric identities do not extend native object lifetime. This candidate
is observation only: native setter activation was withheld because missing
callbacks/changed cameras also require a proven restore/residual contract.
No native peripheral-culling fix or FPS improvement is claimed here. Existing
right-eye projection and diagnostic RT-lifetime fixes remain included.

x86 ABI fixture PASS: exact ESI/caller, original API once, BOOL/output/LastError,
x87/SSE restoration, concurrent bypass and unchanged test EXE text. Projection
witness tests reject wrong frames, non-world uploads and changed identities.
All new pins match the installed D46 EXE. Production and diagnostic builds PASS.
Physical headset acceptance and real native-boundary corroboration remain pending.

## 2026-10-28: native camera lifetime and boundary rejection evidence

935f889 real startup confirms external QPC installation and ESI=000E3A60 on
render thread23616. Present forwarding returns D3D_OK; output/depth2676x2728.
It does not yet corroborate the camera inverse in a gameplay frame. Subsequent
offline code reports bounded rejection stages (view table, renderer reference,
camera reference, camera contract), plus periodic boundary state, instead of
silently dropping every failed ownership traversal. Diagnostic build PASS;
this follow-up has not replaced the running DLL.

Further SHA-pinned Ghidra analysis changes the lifetime assumption: 84BBA0
increments reference-cell+4 and releases the old cell; 402550 puts the unused
cell on a free list. Neither function retains/destroys the camera object.
Consequently these calls do NOT prove a strong camera lease across native
rendering. Do not enable a restore-after-render implementation by merely
incrementing this counter. Current observation dereferences only synchronously
inside the native callback and retains numeric identities, not object pointers.
71F070 dispatches global010E1388 virtual+28; 706250 is an empty thunk. This
narrows the update-order inspection but does not prove the entire render order.
Private disassembly remains outside Git. Peripheral fix remains incomplete.

## 2026-10-28: camera destructor boundary follow-up

Installed D46 camera vtable FF2800 slot0 resolves to709270. Its deleting
destructor releases +7D4, calls CBA920 and A672B0, then optionally frees the
object. A672B0 ends in934E50 ->6BBEF0. 934E50 distinguishes cell+8 ownership
from cell+4 handles: on the last owning reference it clears the cell object
pointer before calling its destructor. 6BBEF0 instead clears object+4/+8 and
releases the cell at object+C; it does not itself write *cell=0. Thus observing
934E50's owning-container path alone is insufficient to assume every direct
camera destruction/reconstruction invalidates a retained weak cell. The camera
also has a previously traced in-place reconstruction path BB5900. A weak-cell
identity is not yet an adequate generation guard for scoped FOV restoration.
No extra native reference operations or camera mutations were installed.
Game was closed at this turn's authoritative process check. This is further
offline evidence, not a peripheral-pop-in fix or runtime validation.

## 2026-10-28: separate native culling input

Implement native_culling_cover.h: validate the live native view/projection/VP
composition, then prepare a separate binocular-covering VP and inverse without
editing FOV or any render matrix. Connect this preparation to the current owned
camera callback in observation mode. x86 regression PASS100 translated/rotated
cameras, binocular angular coverage, inverse agreement, byte-preserved inputs,
and unchanged output on mismatched native VP. Diagnostic DLL build PASS.

4BE0C0 rebuilds camera+620 frustum via607CD0, inverse viaBBE8F0, then685050;
the render VP at+500 is separately written earlier. 607CD0 extracts normalized
planes, including near/far classifications, using62DD40. The frustum embeds
vector headers at+30 and+150 and may own heap storage: shallow copying its
416-byte region would alias plane/type buffers. No such copy is implemented.
This identifies a culling-only implementation route that avoids retaining or
restoring a temporarily widened camera FOV across render. Native frustum writes
are not connected yet; matrix preparation alone does not fix pop-in.

## 2026-10-28: opt-in native culling-only candidate

Render/NativeViewBoundary=2 now connects the separate widened VP to the native
607CD0 plane rebuild and685050 bounds rebuild, exactly the two thiscall calls
issued by4BE0C0. It requires a fresh preceding primary-world inverse witness,
the current view/renderer/camera identity, matching current native VP, and no
3D camera uploads yet in this frame. Mode1 observes;0 disables at startup.
No FOV setter, render projection/VP change, camera retention or EXE write.
An absent callback or failed gate leaves baseline native culling for that frame.

The existing plane and classification vectors must have six entries, sufficient
capacity, finite planes, alignment16 and native near/side/far tags1/0/2. Native
rebuild reuses those buffers; never shallow-copy their owning headers. Aligned
scratch VP/inverse use the separately tested math. BBE8F0 confirms inverse
output is sixteen floats; D37570 projects eight clip-box corners with that
matrix when685050 rebuilds bounds. Native algorithms themselves are not tested
by the offline doubles.

Actual-source x86 fixture PASS1000 calls: native ABI/order/alignment, original
plane-buffer identities, unchanged camera bytes outside frustum, rejection of
bad capacity, tags, counts and NaN. Pure covering math PASS100 cameras; both DLL
builds PASS. Runtime application must be witnessed by NativeCullingCover lines;
physical side-pop-in, lighting, right-eye stability and FPS remain NOT RUN.

## 2026-10-28: runtime rejects late boundary; move before update dispatch

98aebd7 gameplay logs show world=1, views=1 and had3D=1 repeatedly atA6677B
(frames6598..9598). No NativeCullingCover application is established. The guard
correctly refused writing after drawing; this runtime contradicts the earlier
assumption that the final +B0 QPC precedes world rendering. Select first QPC
return A66486 instead, before view virtual+114/AC6E70 and later update dispatches.
Pin its exact call/continuation bytes too. Native callbacks retain the existing
before-3D and fresh-camera gates. Whether later native updates overwrite this
earlier frustum remains a runtime question, not an assumed fix.

Expanded x86 geometry regression PASS64800 world-space edge/depth points across
100 translated/rotated cameras,53600 outside the original cone. Near/far depth
mapping and render inputs remain unchanged; mismatched, singular and NaN camera
inputs reject without changing output. External ESI ABI fixture and diagnostic
build PASS. No headset or FPS acceptance is claimed.

## 2026-10-28: distinguish camera uploads from submitted world draws

5b30d19 startup/transition logs establish the earlier callback can traverse
FC8930 ->renderer ->camera and prepare a valid culling VP (FOV45 ->123.92),
but do not corroborate/apply it. In later world frames had3D is again1. Source
inspection corrects the interpretation of that flag: Native_SetVSConstantF sets
it on recognized camera matrix uploads, not on a draw. Hence had3D alone does
not prove either QPC site is after actual world drawing.

Add a separate conservative world-draw-issued flag at the four actual live
draw forwarding functions, after diagnostic skips and before the driver call.
Replay, depth-only shaders, unrecognized WVP and skipped draws do not mark it;
a failed driver call still blocks later mutation conservatively. Reset it at
the same frame boundary as the camera-upload flag. Native view observation
now runs after camera uploads too; only writes require no issued world draw.
Bounded logs expose both states. Both x86 builds and actual-source culling
rebuild1000 checks PASS. No runtime evidence yet for this follow-up; running
candidate remains5b30d19 until a later controlled deployment.

## 2026-10-28: contiguous constant-window flush

Track whether each window bank's written union is provably contiguous. Empty,
overlapping and adjacent uploads preserve that proof; a disjoint upload disables
the shortcut until Flush/Reset (later hole filling remains conservative). Flush
of a proven contiguous span clears its dirty bytes in one memset and sends the
same register range once, replacing the per-register branch/clear loop. Sparse
ranges, VS/PS independence, failure disabling and consume-before-driver semantics
remain unchanged. This optimizes CPU bookkeeping without eliding D3D calls.
Actual-source production/diagnostic replay tests PASS576096 records/70090 barrier
snapshots each, including transformed bits, sparse ranges and faults. Both DLL
builds PASS. No measured runtime or FPS improvement is claimed.

## 2026-10-28: private instance entry before native view rendering

Runtime 3f7e644 reaches gameplay FOV60 with worldDraw=1 at A66486; QPC is
therefore discovery/observation only. It cannot safely establish the intended
pre-render mutation phase. Add a private read-only clone of all82 FC8930 slots
and its preceding header, replacing only instance slot+A8 with a state-preserving
tail wrapper to the original99F1B0 ->888720 ->985D80 path. SHA-pinned table
boundaries FC892C/FC8A78, slot0, slot42 and slot81 are checked; every copied
target must belong to executable game-image memory. The clone is prepared once,
published atomically only to a matching aligned writable private-heap instance,
and retained for process lifetime. No instance address is retained, no stale
restoration runs, and C5BFA0's original destructor republishes FC8930. Original
EXE, IAT and static tables are unchanged. This introduces a heap-instance vptr
change, explicitly distinct from forbidden writes to hashed static tables.

Only the +A8 wrapper can call the culling rebuild; QPC cannot. Matrix ownership
still requires a fresh matching live inverse upload in the preceding frame.
Accept either order of upload and boundary within the same frame, retaining
the exact frame and numeric identities; old frames and mismatches reject.

x86 instance regression PASS1000 publications/reconstructions, ECX/EAX forwarding,
all82 unchanged slots except+A8, header preservation, read-only clone, original
bytes and foreign-vptr/guard rejection. External ESI/witness regression PASS
including both upload orders; actual-source culling1000 ABI/alignment checks
PASS. Production and diagnostic builds PASS. Runtime activation, persistence
through native camera updates, side coverage and headset/FPS acceptance remain
unproven until the new candidate runs in the scene.

## 2026-10-28: keep the pre-render witness isolated from QPC discovery

5af8536 runtime confirms the private instance dispatch and D3D_OK Present;
the current observed scene still has no live inverse-projection upload, so no
culling write or visual fix is proven. Keep this distinction explicit.

Once the private table owns the instance, QPC now returns after discovery.
It cannot replace the pre-render candidate with a camera state updated later
in the render pass. An already-owned immutable vptr bypasses VirtualQuery and
atomic republishing too. Actual-source x86 dispatch regression PASS10000 owned
callbacks without prepare/publish/late observation; failed admission, observe
mode and foreign-thread cases PASS. The external ABI/witness regression and
diagnostic build PASS. This follow-up is offline; the running DLL is5af8536.

The initial dispatch fixture executable name triggered Windows elevation
heuristics and did not run. No UAC was invoked. Rebuilt as view-phase.exe;
explicit exit-code reset now rejects a missing launch, and that run prints PASS.

## 2026-10-28: fit culling to the actual tracked stereo matrices

The installed profile has NativeStereoHeadGeometry=1 and HeadGeometry=1.
The prior angular union ignored that active head rotation/translation and the
eye baseline. PrepareStereo now uses the same eye-matrix compensation as live
geometry, fits both eye volumes in camera space, and composes an off-axis clip
X/Y fit with the reference eye projection. It preserves the shared depth/horizon
planes and rejects mismatched depth, non-affine native views, singular/NaN
matrices and unbounded inverse corners. The reference eye is included with its
exact [-1,1] bounds, avoiding an unnecessary inverse/multiply. No render P/V/VP,
camera FOV or lighting-reference point is modified by preparing these inputs.

The write gate now also compares the current full inverse projection against
both the freshly verified native candidate and the latest live projection.
A changed FOV/clip interval therefore cannot reuse an earlier projection proof.
x86 geometry PASS240 tracked poses,249600 binocular points,201581 outside the
original cone; rotations/translations, asymmetric scales, eye baseline and
translation/shear modes included. Existing64800 angular points, actual-source
culling1000 ABI/order calls, external boundary/witness and phase-isolation
regressions PASS. Production and diagnostic builds PASS (final header follow-up
rebuilt diagnostic). Native plane algorithms and physical HMD behavior are not
emulated by these tests.

Offline Ghidra checks establish another integration risk: within985D80, the
conditional98602E call to745E20 unconditionally rebuilds P and frustum via
BA9150/4BE0C0 before98618E/98619A copy that frustum. Thus a pre888720 change can
be overwritten when that branch executes. Whether it executes in the user's
worst scene is not established by this static evidence. Do not claim that the
new union alone fixes this phase problem. A777C0 reads camera+7C0 into the
render context before the copy; that lighting/reference position remains native.
Private decompilation stays outside Git.

User steering: no game launch until explicitly requested; continue offline
implementation without waiting for headset tests. No deployment/launch is made
for this change. Current installed candidate remains5af8536.

## 2026-10-28: apply stereo culling immediately before the final native copy

Replace mode2's private instance render-entry interception with the exact
98618E boundary inside985D80, after the conditional745E20 camera rebuild and
before9962D0/935A70 expose/copy the frustum. Pinned instructions load the
selected camera into ECX through the reference field at EDI; its owning
renderer is EDI-3F4. Revalidate that relationship and camera contract while
the native caller owns them. No borrowed object pointer survives the callback.
QPC mode1 remains observation-only and cannot mutate culling or replace a
mode2 witness. Production no longer uses the private view-table prototype.

The boundary uses one local x86 execution debug slot and a narrowly filtered
VEH on the render thread. It leaves EXE code, IAT, static tables and instance
vptrs unchanged. The handler resumes the original instruction with RF and
preserves LastError/x87/SSE; unrelated exceptions continue searching. Startup
refuses debugger/occupied debug slots. Thread context changes use a helper
that suspends only its own process's calling thread, performs context API
operations, then resumes it. The module/state remain alive for the process;
teardown is same-thread/quiescent only and restores only its owned slot.
This is an opt-in diagnostic candidate, not an OS/dispatcher modification.

The culling union uses the actual tracked stereo projection matrices from
ff29a2e. The write still requires fresh primary-camera projection evidence,
matching numeric camera/renderer identity, unchanged full inverse projection,
finite bounded geometry, and the exact final-copy boundary. Native607CD0 and
685050 rebuild only owned planes/bounds; render matrices, FOV and camera+7C0
remain unchanged. No speculative ownership or invalidation bypass was added.

Offline x86 PASS: 10000 execution hits with integer/flags/LastError/x87/SSE
preservation, other-thread bypass/Stop refusal, occupied-slot refusal,
unmatched single-step passthrough, foreign-slot preservation on Stop,
20 rearm cycles and unchanged fixture EXE text. Synthetic mean was7-16us/hit
across runs, not a measured game cost or FPS benefit. Actual-source routing
PASS10000 final-copy callbacks with exact EDI/ECX ownership, wrong phase/thread,
closed frame, unsupported render modes and invalid owner rejection.
Geometry PASS249600 stereo points across240 poses plus64800 angular points;
actual-source native-call ABI/order/alignment PASS1000 and external observer/
projection-witness regression PASS. Production and diagnostic DLL builds PASS.
Installed EXE SHA and21 final-copy instruction bytes verified read-only.

Headset side-pop, right-eye flicker, actual native plane execution and FPS
acceptance remain NOT RUN for this candidate. Prepare it for the user's
requested test without launching the game. Preserve the prior DLL/profiles
and native2676x2728 source resolution for rollback and comparison.

## 2026-10-28: camera-only segments retain the right-eye background

The right-eye color-history seed used g_had3DCameraThisFrame to decide whether
to regenerate world pixels. That flag records a camera constant upload, not
a world draw. A camera-only/UI segment could therefore skip restoring the
existing right-eye history and keep the live left-eye background. Gate on
the separately tracked g_hadWorldDrawThisFrame instead. That flag is set at
the four live world draw forwarding paths, after diagnostic skips, and reset
at the existing XR frame boundary. No new frame/camera inference is added.
World draws still bypass the copy and regenerate their right-eye pixels;
camera-only segments restore only history for the same owned render target.

An x86 fixture compiles the actual EyeColorHistory/seed implementation and
its actual call-site expression. The committed1cea63a source reproduces an
expected FAIL: camera constants but no world draw leaves left pixels instead
of the saved right background. The correction passes1000 world/camera-only/UI
cycles, no extra copy for world draws, creation/copy failure and target change
ownership tests. Pixels/API results are simulated; this does not establish
that the user's street flicker follows that path or prove a physical fix.
Production and diagnostic x86 builds PASS. No game launch or runtime test.
Keep the installed1cea63a side-culling candidate stable while preparing this
additional correction offline; do not silently replace its test baseline.

## 2026-10-28: original native frustum builders verified under x86 emulation

Close part of the previous native-algorithm coverage gap without launching the
game: test/native_frustum_emulation.py reads only the SHA-pinned installed EXE
and emulates607CD0 followed by685050. No entrypoint, OS API, allocator stub,
instruction skip, EXE write or runtime attach. Preallocate exactly six plane/
type entries as required by the producer contract. Reject any emulated write
outside the owned frustum/vectors/private stack, including all image writes.
Check original thiscall stack cleanup, input matrix preservation, unchanged
vector pointers/capacities/types and both native bounds flags.

native_stereo_culling_test.exe optionally exports only our generated VP,
inverseVP and binocular homogeneous points to a private binary input.240 poses,
249600 points,480 original function calls PASS. Independently compare actual
native outputs to D3D clip planes and inverse-corner bounds in double precision:
worst coefficient error/(1+abs(expected))1.683e-7 (limit2e-6), bounds error1.466e-6
(limit1e-5). This confirms the native plane sign/order, matrix convention and
AABB construction for the generated stereo inputs. No production algorithm
replacement is justified by these results.

The old geometry test's loose0.003*(1+worldMagnitude) boundary tolerance also
passes, but is not exact containment: raw far-edge excursions reach3.006 world
units against planes and3.448 against bounds. Report these rather than hiding
them behind PASS. Floating projection/inverse/world transformations can move
far corners slightly; this does not prove their relevance to visible street
pop-in or justify changing clip planes speculatively.

Reproduce after build_native_culling_geometry.ps1 by running its generated
native_stereo_culling_test.exe with one output-path argument, then Python
native_frustum_emulation.py --exe <installed-D46-EXE> --cases <that-output>.
Dependencies: pefile, numpy, unicorn2.1.4; optional --dependency-root points to
an isolated dependency directory, --report writes a private JSON result.
No dependency binaries, EXE bytes or decompilation are included in Git.
Unicorn API reference: https://www.unicorn-engine.org/docs/tutorial.html
Native scene ownership/activation and HMD side-pop/right-eye/FPS acceptance
remain NOT RUN. Installed1cea63a and prepared8c2d6e1 remain unchanged.

## 2026-10-28: reject partial right-eye replays instead of saving/submitting them

ReplayStream previously swallowed a per-record SEH fault and returned void;
RenderEyePairFromReplay then saved the resulting partial right-eye pixels to
history and could mark the pair ready. Float constant setter and draw HRESULTs
also did not affect pair validity. Return a completion result that becomes
false on any caught record exception, failed float constant driver submission
or failure from any of the four draw wrappers. Continue existing scene/state
cleanup commands after an exception; do not retry the failing call. Gate right
history Save and capture on that result. Left color/state restoration and
transfer-worker joins still run. The existing same-frame pair validity gate
therefore submits no XR layer for such a frame; it does not publish mixed eyes.
Normal command ordering, native results and successful-frame driver calls stay
the same. A bounded failure message distinguishes this rejection in logs.

Correction to the initial audit lead: VrFrame already excludes g_recOverflow
before invoking RenderEyePairFromReplay. Do not call overflow an exposed bug
or add a redundant second guard. No retained log inspected here establishes
that any replay exception/draw/float setter failure caused the reported street
flicker. This is a proven error-propagation defect, not a proven runtime cause.
Other D3D9 state-call HRESULTs are outside this correction's coverage.

Actual-source x86 float-loop regression PASS in production and diagnostic:
96 streams/576096 records/70090 bank observations per build, batching OFF,
adjacent/window/material-bind/capture cases. Completion must agree with driver
failure status; an injected SEH must return false with no retry. Actual-source
right-history tests PASS1000 cycles plus failed-replay/history rejection,
four draw HRESULTs and matching clip cleanup on success/failure, target change
and copy/create failures. Production and diagnostic DLL builds PASS.

Also inspected snapshot/inherited-shader state. StateBlockHook and LightPosShift
are OFF in the current profile, so their dormant metadata paths do not justify
a visual/performance claim or enabling them. Do not introduce a raw constant
cache without device-bank/state-block ownership and invalidation proof.
No runtime/headset/FPS claim, no game launch, no installed-baseline replacement.

## 2026-10-28: propagate replay surface-operation failures

Extend the partial-replay rejection to recorded SetRenderTarget,
SetDepthStencilSurface, Clear, StretchRect and ColorFill. Their HRESULTs were
discarded: a failed target change/copy/clear could leave old or incorrectly
targeted pixels while the caller still saved right history and published the
pair. Each original operation still runs once, in its existing order; failure
sets the same sticky replayComplete result used by history/publication gates.
Later scene/cleanup commands continue. Successful nonzero HRESULTs remain valid.
No retry, extra driver operation, cache, native-image write or scene bypass.
This is a proven error-propagation defect, not evidence that these APIs failed
in the user's scene. Other state setters and snapshot Apply remain outside the
completion check; do not describe this as exhaustive replay validation.

New actual-source x86 surface fixture PASS:96 streams/576 calls. Inject failure
individually at each of five operations; require false through the final
EndScene, exact arguments and ordering, no retries, null target/depth bindings,
all source/destination rectangle masks, D3D_OK and S_FALSE. Removing only the
five checks in the generated fixture reproduces the expected failure. Existing
history1000 cycles and draw HRESULT checks PASS; production and diagnostic
constant-loop576096 records each PASS; production and diagnostic DLL builds
PASS. No game launch or runtime/FPS claim. Installed6e7d104 remains unchanged
while this follow-up is packaged separately for offline review.

## 2026-10-28: invalidate obsolete right history across failed segments

The previous rejection guards prevented saving a failed replay but retained
the preceding right history. The live left world may already have advanced.
A following camera-only/UI segment could restore that obsolete right world
and publish it beside the newer left world after its own successful replay.
This is a cross-segment error path in the guards, not a captured runtime cause.

FinishRightEyeHistory now invalidates the history on failed replay or failed
background seeding. Save also invalidates on allocation/description/copy failure.
Restore refuses rejected history, including after a target switch. Rejection
persists across UI segments; a successful fresh world replay/Save recovers it.
Explicit Reset Release clears the rejection and preserves initial-menu behavior.
No old-eye fallback, retry or additional successful-frame surface copy. A failure
followed only by UI can suppress pairs until the next world render or Reset;
this is intentional rather than publishing different generations of the world.
Other snapshot/state errors and runtime HMD acceptance remain unproven.

Actual-source x86 history PASS1000 normal segments +1000 rejected UI segments,
world recovery, target changes, create/copy failure, Reset and all four draw
results. Negative control removing only failure invalidation reproduces the
obsolete-history failure. Production and diagnostic builds PASS.

Bounded side-culling numeric check: original native builders in emulator across
the existing240 poses/249600 points have maximum side-plane excursions below
0.000159 world units (2.391e-7 relative to sampled point magnitude). The prior
3.006-unit worst plane excursion is the far plane, not a lateral plane.
This does not justify guessing a much wider lateral margin. Native AABB raw
excursion remains3.448 units and the original loose fixture is not exact
containment proof. No culling arithmetic changed on the strength of this check.
No game launch or FPS claim. Follow-up packaged separately from installed6e7d104.

## 2026-10-28: verify history on real x86 D3D9 REF surfaces

New isolated driver fixture compiles the actual EyeColorHistory, seed and
completion implementations, loads system D3D9 by absolute system path, creates
a hidden window and a REF device. It never loads the mod, game or OpenXR.
PASS42 cycles across64x48,257x129 and native2676x2728 surfaces: full pixel readback
verifies left preservation for world segments, right restoration for camera/UI,
8x8 overlay persistence, rejection across failed replay/UI segments and recovery
on new world replay. A real unsupported SYSTEMMEM-to-render-target StretchRect
fails and invalidates history; fresh world replay recovers. Three device Reset
cycles pass after explicit release. This covers single-sample A8R8G8B8 REF
surfaces; no HAL, MSAA, game ownership, headset acceptance or FPS claim.

The driver test changes no production source. Package/deploy the combined
9274cb9 production changes only while DarknessII is absent, retain profiles and
rollback, and do not launch the game. Installed status is recorded in the
local runtime bundle rather than inferred from this test.

## 2026-10-28: enable strong same-resolution NIS and validate the actual eye host

User explicitly requests strong NIS from Singularity alongside side visibility.
Darkness2VR already vendors the same NVIDIA v1.0.3 embedded sharpen shader:
NIS_Sharpen_CS.h SHA256 E09965870E95382AB689DF96E5FA3461DF19C7FD79D492D4BA247567AE6489E1
matches the current local SingularityVR source. Reuse this implementation and
set the requested candidate to NisMode=2,NisSharpness=0.80. Mode2 preserves
native source/output dimensions; it adds compute sharpening, not render scaling
or a replacement for geometry/culling fixes. Repository sample config now
exposes those settings. No proprietary game data or Singularity source changed.

Fix a host failure path before enabling it: failed SRV/output creation or
dispatch could otherwise be retried on every submission. Latch NIS host failure
until device Reset/shutdown; return null output pointers on failure, leaving
the existing pair fallback in charge. A bounded first-success message records
actual dispatch mode, strength and dimensions rather than startup intent alone.
Successful frames retain SRVs/UAVs and issue no new input-eye copy. Existing
final copy to XR remains. No per-frame compilation or resource creation at
fixed resolution. NIS is additional GPU work; no FPS improvement is claimed.

New x86 WARP fixture includes actual nis_eye.inl, not a duplicate dispatch.
PASS independent66x35 eyes with distinct colours and all borders checked,
20%vs80% changes2070 actual pixels, source changes do not contaminate eye1,
persistent resources across10 frames, real invalid-SRV failure,1000 rejected
subsequent attempts, shutdown/recovery, full native2676x2728 pixel readback
for both eyes. Existing culling regressions PASS100 cameras/64800 world-space
points and240 tracked poses/249600 binocular points. Production and diagnostic
builds PASS. WARP proves pixel behavior, not game activation, headset quality,
GPU cost on the user's card or90FPS. Do not launch the game.

## 2026-10-28: compare native culling against original-eye double-precision corners

Extend the generated-case export with optional --reference-eyes (D2FRC002).
It retains the original native-view matrix and both actual eye projections,
in addition to the existing fitted VP/inverse and float-generated points.
No executable bytes or game assets are exported. Legacy D2FRC001 stays valid.
The emulator now optionally inverts the original view/eyes in double precision
and measures native planes/AABB against all16 actual eye corners per pose.
This avoids treating the rounded fitted inverse or float test points as the
ground truth for strict containment. It does not change production rendering.

Original native x86 builders,240 poses/480 calls: near-plane maximum excursion
0.0001454; side planes at most0.0001466; far-plane2.85024; AABB3.78415 world units.
210 poses have some plane excursion above1e-4,62 some AABB excursion above1e-4.
Strict corner containment is false. The legacy PASS still explicitly refers
only to its existing loose geometry/oracle tolerances; the diagnostic reference
is separately reported and must not be described as strict containment PASS.
These synthetic far-boundary errors are not proof of the broad side popup's
runtime cause and do not justify guessed lateral expansion or native reference
point changes. No new geometry adjustment or installed-DLL replacement here.

Both generated geometry fixtures PASS their existing scopes. D2FRC002 emulator
run completed against the pinned D46 executable with the above diagnostics.
Keep the installed6c39998 NIS80 candidate and rollback intact; no game launch.

## 2026-10-28: conservatively include binocular corners in native AABB

PrepareStereo now derives a separate world AABB from the16 original per-eye
clip corners, with camera-local/eye inverses and compositions in double
precision. Round extrema outward to float using nextafter. The native rebuilt
AABB previously excluded some of those reference corners by up to3.78415 world
units in the synthetic translated/rotated corpus. After native607CD0/685050,
expand only the owned AABB XYZ minima/maxima to also include these bounds.
Never shrink native bounds. W, flags, owning vectors, plane equations, camera
reference point, FOV and render V/P/VP remain unchanged. Invalid stereo bounds
are rejected before either native builder. This corrects an AABB containment
defect; far-plane equation error remains, and broad side-popup causality is
not proven. No EXE/IAT/vtable patch and no arbitrary world-space padding.

Avoid the added inversions for unused candidates: construct the stereo input
only after the existing exact-copy-phase, installed-hook, mode and projection
corroboration predicates all pass. Previously unmatched/observation cameras
still computed discarded candidates. Witness observation/update remains in
place. No runtime CPU saving or90FPS claim.

Extend optional export to D2FRC003 (D2FRC001/002 still readable) and add a
bounded binary helper mode to the actual C++ geometry fixture. The emulator
runs original native builders then feeds their output and the generated
candidate bounds to the actual C++ ExpandOwnedBounds helper in one batch.
Independent NumPy double original-eye corners are strictly inside corrected
AABBs in all240 poses; native bounds never shrink and both W fields match.
Plane results are unchanged and strict full-frustum containment still fails
at the previously reported far boundary. Do not label that failure fixed.

Actual-source rebuild ABI fixture PASS1000 legacy +1000 expanded rebuilds,
render/reference/W preservation, invalid-input-before-call rejection and
malformed-vector rejection. Phase routing10000 PASS; geometry64800+249600
points PASS; production and diagnostic builds PASS. No game launch, headset
or performance claim. Keep NIS80 and native2676x2728 in the combined candidate.

## 2026-10-28: include actual binocular corners in native plane half-spaces

Retain the16 already-computed double world corners in the prepared input.
For each of the six native planes, keep its exact XYZ normal and reduce its
offset only if needed to contain those corners, rounding the support value
outward to float. Native convention is dot(normal,world)-offset >=0 inside;
the emulator's independent D3D plane oracle verifies this representation.
Thus every correction preserves or widens a native half-space, without a
guessed far-distance constant, altered normal, render projection or FOV.
Expand bounds and offsets together only after validating all values. Reject
nonfinite/out-of-bounds corners before native calls. Keep types, vector
ownership, flags, bounds W and other camera fields unchanged. No EXE writes.

Extend generated reference format to D2FRC004 with our generated double
corners; older formats and bounds-only helper remain supported. Original
native builder output goes through actual C++ ExpandOwnedFrustum in a single
bounded batch. Both AABB and all six planes strictly contain independent
NumPy original-eye double corners in240 normal-range poses and240 additional
full-rotation poses (yaw/roll ±pi,pitch ±1.5). Normals are bit-identical and
no plane/bound shrinks. Geometry fixture still checks249600 points per corpus.
The widened-angle original output FAILS its loose legacy tolerance at one
pose (case12); report that baseline failure explicitly rather than labeling
the original output PASS. Full-correction mode instead requires strict
independent-reference containment after actual C++ correction, which passes.
Full-rotation original raw reference errors reach3.09294 plane/5.36730 AABB
world units. These are synthetic data and do not prove runtime popup causality.

Additional tests cover nonfinite last-plane data, zero normal, malformed bounds,
nonfinite/out-of-bounds corners: every failure leaves all helper output bytes
unchanged. Already-wider native planes remain wider. Actual-source ABI fixture
PASS1000 legacy+1000 corrected rebuilds, normals/W/render/reference preservation
and malformed-input rejection. Production and diagnostic builds PASS. No
headset/FPS claim or game launch. Combined candidate retains NIS80,2676x2728
and rollback. Other engine visibility/occlusion filters remain unproven.

## 2026-10-28: executable-page compatibility and exact boundary refusal status

Retained gameplay log last written13:15 local reports NativeFrustumBoundary
installed=0 at0098618E and no NativeCullingCover/beforeCopy corroboration.
That run therefore did not activate the final binocular culling correction.
It predates the NIS80 and conservative-plane candidates; it is not runtime
validation of either. The old log does not expose the installation failure
stage, so its exact cause remains unknown.

The boundary validator accepted only EXECUTE/EXECUTE_READ. It now accepts
EXECUTE_READWRITE/EXECUTE_WRITECOPY as well: the observer never writes code.
Retain committed MEM_IMAGE, exact allocation base/image bounds, no guard or
noaccess pages, debugger refusal and refusal of any foreign debug-slot state.
Add one bounded installation status with failing stage, Win32 error, observed
page protection and DR0-3/DR7 snapshot. Worker collects these using fixed stack
data; no logging, allocation or loader calls while the owner is suspended.
No retry loop, stolen debug slot, EXE/IAT/vtable write or launch.

Own x86 fixture reproduced the old validator failure after requesting RWX;
the corrected actual-source fixture passes10023 callbacks including requests
for both writable executable protections (Windows may normalize image-copy
protection). Checks also verify refusal stages, foreign DR1 preservation,
registers/flags/LastError/x87/SSE, other-thread bypass,20 rearm cycles and
unchanged text bytes. Actual-source final-copy routing10000 PASS. Production
and diagnostic builds PASS. Preserve2676x2728 per-eye native rendering and
NIS80 in the reversible candidate. Game activation, side visibility and FPS
still require the user's next runtime test; no90FPS or popup-fix claim.

## 2026-10-28: reuse the privately owned NIS configuration buffer contents

Remove Map(WRITE_DISCARD)/Unmap when the complete NIS configuration bytes
already match the last successful upload. Both zero-origin independent eyes
use the same config at matching dimensions/sharpness. Only NisDispatchEye
writes this private buffer; shader reads and pipeline rebinding cannot edit it.
Keep all bindings/dispatches, both eye images and official shader bytecode.
Zero the full256-byte aligned config (including padding) and publish the
receipt only after successful upload. Changed config uploads once; failed
Map invalidates the receipt; shutdown destroys the buffer and resets it.

Actual x86 WARP host PASS:22 dispatches use one successful config upload;
16 full-image comparisons against forced fresh driver uploads are identical
across strength changes/clamping, sharpen/scaler and output resizing. Real
Map failure with a non-mappable fixture buffer leaves no receipt, then the
restored private buffer uploads successfully and matches prior pixels. Reset
and failure-latch tests retain the original coverage, including full native
2676x2728 readback. Production/diagnostic builds PASS. This removes two stable
per-frame Map/Unmap pairs; no measured game CPU/GPU saving or90FPS claim.
Leave installed7ef2726 unchanged for its user test.

New log written14:00 local identifies the actual culling refusal:
occupied-debug-state, protect00000020, DR0=0112263C DR1=011223F0
DR2=01122878 DR3=01838534 DR7=FFFF0015. Thus writable-page compatibility
was not this run's blocker; no NativeCullingCover callback ran. Do not take
the foreign debug state or claim side correction active. Same log confirms
embedded official NIS mode2 strength0.80 dispatched at2676x2728 per eye.
It does not establish visual/HMD or90FPS acceptance.

Read-only disassembly of the same SHA-pinned installed EXE identifies the
native owner contract: E35880 writes CONTEXT.Dr0/1/2 from this+238/+23C/+240,
CONTEXT.Dr3=this and CONTEXT.Dr7=FFFF0015. Native VEH E355E0 reads the context
DR3 field and invokes that object's virtual+8 when its own predicates pass.
Thus the disabled slot3 is not free storage; replacing its nonzero address
would also replace the handler's object. Preserve the refusal, not just the
enabled-slot bits. Raw opcode substring matches in unrelated functions were
discarded after aligned disassembly (some lie inside call displacements).
The existing hardware-boundary route cannot be treated as a working culling
solution on this game's native debug-state contract. Need a separate route
to the proven post-rebuild/pre-copy point while retaining native code bytes
and all original debug state. No native handler/dispatcher modification.

## 2026-10-28: experimental owned-method route to final stereo frustum copy

Add opt-in NativeViewBoundary=3. The original hardware route remains unchanged
and refuses occupied debug state. The new route never reads/writes thread
debug contexts. At the existing external QPC observation point A66486, publish
an immutable private vtable on the matched heap view instance, replacing only
slot+A8. Preserve the preceding header word and every other entry (82 or77
entries for the two pinned native tables), including native destruction.
No native instance pointer is retained. Code and tables remain pinned for
process lifetime. No EXE/IAT/static vtable bytes are changed.

Copy the two exact SHA-pinned methods888720 and985D80 into owned storage.
Relocate131 audited E8 calls, retain internal branch offsets, and redirect
only the copied98618E getter call through the existing register/flags/x87/SSE
preserving shim. The callback is after the native camera rebuild and before
935A70 copies the frustum. It uses the existing camera/projection witness and
conservative binocular plane/bounds correction; it does not widen render FOV
or skip geometry. Seal copies RX before publishing any entry. Validate live
function digests and complete table digests; any mismatch refuses activation.
Repeated table preparation must match source/count/slot/replacement exactly.

Read-only Capstone CFG audit PASS:461+1110 instructions,4 returns each,
29+102 relative calls, no indirect jumps, escaping branches, local FS/GS SEH,
or explicit copied-code references. Generated metadata is byte reproducible
from the pinned EXE; native bytes/decompilation are not committed.

Offline x86 PASS:10000 owned copied-method callbacks with cross-block and
external calls; source bytes and foreign debug state preserved; RW-to-RX and
digest/plan refusals. Private table1000 publications/reconstructions preserve
82 entries/header and ECX/EAX. Actual-source routing10000 hardware callbacks,
10000 translated callbacks and10000 publication observations; invalid phase,
thread, mode and owner gates refuse, LastError preserved. Real x86 shim7126
checks PASS. Actual-source culling rebuild1000 native+1000 corrected calls
PASS. Production and diagnostic builds PASS. Existing third-party HDE emits
C4701 in its fixture build; no new production warning from this route.

This remains an experimental compatibility candidate: copied calls have new
return addresses and execute outside the original image range. The native
exception handler filters EIP by image range; fixture debug-state preservation
does not prove compatibility with native protection, indirect watched-memory
access, or return-address-sensitive callees. No handler/range bypass is added.
Only the user's real-game test can establish activation, stability and side
visibility. Native2676x2728 per eye and NIS80 are retained; unchanged private
NIS config upload reuse is also included. No measured FPS gain or90FPS claim.

## 2026-10-28: connect bounded asynchronous NIS GPU timestamps (offline only)

The retained log reports native D3D9 GPU windows near10-11ms but has no NIS
GPU result. The existing optional timestamp queries were allocated and never
used. Bracket the two eye dispatches, excluding final copies/XR; poll once
at the next successful pair, with DONOTFLUSH and no wait/retry loop. One set
stays pending until consumed; never overwrite it or nest Begin. End is inert
without Begin, including failures. Disjoint/zero-frequency/reversed samples
are discarded and GetData errors release the pending receipt. Reset clears
all state. Log the first completed sample and every120 samples, not frames.
The bracket measures GPU elapsed time including scheduling between endpoints,
not exclusive shader execution or an additive CPU stall. Lifetime peak mixes
workloads; compare only matched scene/size/config samples.

Actual x86 WARP host PASS including the full-image previous NIS regressions,
native2676x2728, pending Begin rejection, nested Begin/unmatched End guards,
real timestamp completion and reset. Only the fixture explicitly Flushes to
submit its standalone workload; production polling uses DONOTFLUSH. WARP
timestamp values are not NVIDIA hardware/game performance evidence. Both
builds PASS. No shader/pixel/resolution changes, no new general profiler and
no launch. Installed9106f53 remains unchanged for the user's side-visibility
test; this instrumentation is not in that candidate.

## 2026-10-28: forbid implicit flush while polling already submitted readers

Both D3D11 reader-event polling paths used GetData flags0 despite an explicit
Flush immediately before their loops: transfer Job::Run and shared-reader
Fence::PollIssued via SharedReaderFenceOps. Set DONOTFLUSH in these two calls.
The exact query has already been Ended/submitted; keep the explicit Flush,
same pending dependency, S_OK requirement, error/timeout handling, worker join
and resource retirement. This forbids driver-initiated implicit flush during
polling; it does not eliminate the necessary GPU completion wait or prove that
flags0 actually flushed on this driver. Native D3D9 producer polling unchanged.

Actual isolated x86 D3D9Ex/D3D11 regression PASS1000 transfers,400 at2676x2728,
sampled pixels, concurrent D3D9 work, immediate/deferred readers, failure latch,
RAII join and Reset/recreation. Test reader Ops uses the same DONOTFLUSH flag.
Shared-reader unit regressions PASS including later unrelated commands,
wrong owner, reuse, timeout/error and Reset. Production/diagnostic builds PASS.
No game/Steam/OpenXR launch, no measured game saving. Installed9106f53 is
unchanged; reserve this change for the next candidate.

Side-visibility follow-up: retained935A70 evidence confirms deep copy of the
owning plane/type vectors plus bounds from camera+620. A777C0 separately reads
camera+7C0; it is not a proved removable visibility filter. Do not change that
reference or disable occlusion without its consumer contract/runtime evidence.
Current gameplay log remains the14:00 pre-9106f53 run, so it cannot establish
activation or rejection of the new owned-method side correction.

## 2026-10-28: opt-in D3D11-only transfer worker with native D3D9 flags

Add startup StereoTransferWorker=3, leaving default/profile values unchanged.
Mode1 needs D3DCREATE_MULTITHREADED because its worker polls a D3D9 query in
parallel with replay. Mode3 completes that exact producer event on the owner
before queue publication; its packet retains no D3D9 interface and the worker
only calls D3D11. Factory hooks preserve incoming native flags instead of adding
MULTITHREADED. Never clear a flag requested by the game itself. Mode1 and
serial-MT mode2 retain their old flag policy. Same context lease, reader fence,
same-frame pixels, timeout/failure latch and Reset/teardown gates are retained.
Contradictory producer receipts (complete+query or uncompleted+no query) refuse.

Owner producer wait submits once with D3DGETDATA_FLUSH, then polls flags0;
it still requires S_OK before any D3D11 read. This can reduce global D3D9 lock
overhead at the cost of losing producer-wait overlap with replay. Existing logs
do not quantify that tradeoff; mode3 is an A/B candidate, not a measured saving.

Actual isolated x86 D3D9Ex/D3D11 tests now cover1600 transfers (800 native
2676x2728), both native0x50/D3D11-only and0x54/D3D9-query-worker devices,
concurrent owner D3D9 commands, sampled pixels, deferred/immediate readers,
RAII join, error/fault handling and Reset/recreate. Production/diagnostic builds
PASS. No game/Steam/OpenXR launch or installed profile change. Native culling
and resolution are unchanged; installed9106f53 stays available for the user.

## 2026-10-28: bound the D3D9 MT constant-setter hypothesis

Add a narrow isolated x86 API-cost probe for flags0x50 vs0x54, using the system
D3D9Ex DLL, hidden window, exact creation-flag checks and no draw/Present.
Eight measured alternating-order batches after one warmup:200000 calls/batch,
changing eight VS vectors and two PS vectors, QPC wall plus thread cycles.
No game/OpenXR/Steam calls or attachment, no general profiler. Script refuses
while DarknessII runs and retains raw output in TEMP/d2vr-d3d9-flag-cost.txt.

First local result: median4.629000ms native flags vs7.981250ms MT per200000
setters, delta0.016761ms/1000 calls;87.423 vs151.052 thread cycles/call.
Across85 retained ReplayUploads rows, maximum live-recorded+replayed float
setter count is7376.775/pair. Applying only the synthetic setter delta to
that maximum gives0.123642ms, not a measured game saving. Rows include different
scenes/menu; do not present their median as an outdoor result. Counts exclude
scope setters and this experiment excludes nonconstant APIs/draws/driver-worker
contention. Thus this is not a complete upper bound on MT cost. It does refute
claiming multi-ms savings from these constant setters based on the MT flag
alone. Keep mode3 as an A/B option; do not promote it as the main90FPS fix.

## 2026-10-28: actual RTX5070 NIS dispatch-cost bound at native eye size

Extend the existing NIS host fixture with opt-in -Hardware (default stays WARP).
Same embedded official bytecode, exact host code, NIS mode2/strength0.80 and
independent2676x2728 eyes. Identify actual DXGI adapter and run the existing
pixel/cache/failure/reset tests. Then measure each pair with real D3D11 GPU
timestamps, four warmups and20 samples each for constant images and independent
deterministic noise textures. Explicit submission/wait is fixture-only; runtime
polling remains DONOTFLUSH. No draw/Present/game/OpenXR/Steam or installed change.

NVIDIA GeForce RTX5070: constant mean0.246685ms/median0.231664/max0.493536;
noise mean0.251901ms/median0.251760/max0.253888 per pair. Actual hardware pixel
regressions PASS. Raw output retained in TEMP/d2vr-nis-hardware-cost.txt.
This is isolated dispatch elapsed GPU time, excluding final copies/XR and
game workload contention. It does not establish in-game NIS time or bound
every possible input/clock/scheduling state. Do not add it to a different
scene's timing window. It provides no support for NIS explaining multi-ms
outdoor drops; prioritize native live/replay scene work over NIS shader rewrites.

## 2026-10-28: show actual current-frame culling application in existing label

SubmissionStatus adds a second CULL ON/OFF row when a correcting native
boundary mode2/3 is requested. Green means RebuildOwnedCullingFrustum returned
success for the current open gameplay frame, not merely that the route was
prepared or CPU batching enabled. Clear receipt at frame begin, selected-camera
observation, and before camera reads in final-copy callbacks; failed reads or
rebuilds cannot retain green. Orange means no current successful application.
Modes0/1 hide that row, SubmissionStatus=0 hides both. Existing CPU row unchanged.
Use the same CB/draw/procedural font; no resource, upload, extra draw or EXE edit.
The receipt confirms application, not binocular visual correctness or90FPS.

Actual extracted production PS WARP PASS9 CPU/CULL state combinations with
independent row-color readback. Actual-source phase fixture PASS10000+10000
callbacks, rejected owner reads clear the receipt; both builds PASS. Offline
only, installed9106f53 unchanged. Read-only static check of the two native
method spans finds no direct absolute-memory overlap with the three observed
DR0/1/2 addresses; indirect accesses/protection compatibility remain unproven.

## 2026-10-28: diagnose rejected projection corroboration after user side test

User reports side objects still disappear. The retained runtime log (last write
15:53:05 local, 200843 bytes) shows candidate9106f53 published the private view
and reached98618E, but every sampled application reports corroborated=0 and
cullingApplied=0. During gameplay the previous-frame upload and native inverse
have matching X/Y; the log contains no other matrix components and no successful
corroboration. This establishes an activation failure, not the precise mismatch
or effectiveness of the conservative frustum once applied.

Keep the complete finite16-component tolerance and camera/frame ownership gates.
Add bounded full native/upload inverse pairs, per-component rejection bits,
near/far/aspect, verified-frame identity and upload-phase eligibility. Rejected
uploads log at most once per frame, first3 then every600th rejected frame;
boundary pairs use the existing first3/every600 callback cadence. No new GPU
queries, profiler, native writes, transpose assumption or relaxed clip checks.
Add x86 regression coverage for all16 individual mismatch bits including NaN/Inf;
external API/ABI and actual dispatch routing fixtures PASS. Both builds PASS.
This diagnostic change does not itself repair side visibility or prove90FPS.

## 2026-10-28: isolate raw grip tracking loss per controller

Protected analog grips now consume each controller's own validity. Losing one
controller releases and disarms only its shoulder action. Reacquisition with
grip still held cannot create a fresh grab/slash; release below0.35 rearms that
hand. The other hand retains its hysteresis and active action. Nonfinite and
out-of-range squeeze samples also disarm their own hand. No native targeting,
physics, pose or EXE hook is enabled. Controller-directed throws remain separate
unimplemented integration work in the protected path.

x86 system-XInput forwarding/preservation fixture PASS20041 checks. Actual-source
protected gestures PASS231 checks, including up/down right-hand strokes while
left tracking is absent. Existing pause/focus/rearm and lateral-stroke checks
remain passing. These are offline input checks, not physical headset acceptance.

## 2026-10-28: validate and prepare scoped head-following tentacles

The active game profile explicitly had HeadAttachedTentacles=0 (the local
fallback omits the key and therefore also defaultsOFF), while
HeadGeometry=1 was active in gameplay. Existing scoped head-following was thus
disabled. Add a small override profile enabling that existing path; retain the
three captured VS hashes, seven captured stream offsets, stride40, VB302920,
IB213588 and camera-local WVP gates. Never classify all skinned meshes as the
player. This restores head-relative mesh rendering only, not independent
controller poses, controller targeting or physics throws.

New x86 fixture extracts the actual TentacleHeadScope body. PASS19062 checks,
including1890 shader/stream/eye/tracked-pose combinations, complete constant
restoration, balanced resource references and foreign mesh/shader, shadow,
mode, descriptor/API failure and nonfinite-WVP rejection. This is source/math
evidence, not current-scene mesh attribution or physical-headset acceptance.

Reduce the scope's COM work without caching: reject wrong stream offset/stride
immediately after GetStreamSource, and reject wrong vertex-buffer size before
GetIndices. A rejected stream no longer needs GetIndices or either descriptor;
a rejected VB no longer needs GetIndices/IB descriptor. Tests check actual call
counts. No measured frame-time saving or90FPS claim. Default remainsOFF; the
user-authorized runtime candidate can opt in with rollback.

## 2026-10-28: fix corroboration across the native shader depth conversion

Fresh user-run fb8b30e/PID34996 confirms CULL OFF. Full same-frame world uploads
reject only components11/15 (mask8800): native inverse has -9.99900055,10;
shader inverse has -10.5252628,10.5263157. X/Y agree. This is neither a transpose
nor stale-frame mismatch. Read-only Ghidra analysis identifies AD3380 case1C:
AD4021..AD4067 copies scene+100, scales projection[14] by native constant0.95
atF185C8, then invokes BBE8F0 inversion. Scene+100 receives the selected camera
projection at9861B9/9861C0. The corresponding producer at572337 does the same.

The witness now retains the unmodified native inverse for next-frame camera
identity/projection checks, and independently compares all16 uploaded elements
against the exact expected native shader conversion. Its finite checks and
2e-4 relative acceptance tolerance are unchanged. Current-global shader gating
uses the same expected upload. Camera fields, rendering projections, actual
eye-transform/frustum construction and native output buffers remain unchanged.
Admission also checks the native constant/instruction and SHA256 of the whole
AD4021..AD406C producer span. A modified producer refuses the boundary.

Offline Unicorn execution of the actual AD403E..AD406C block and BBE8F0 matches
all7 logged full matrix pairs, maximum relative component error1.6833e-7.
All emulated writes remain in its private stack. No game starts or attaches,
native code export, actual EXE write, relaxed matrix matching or guessed depth
normalization. x86 witness tests reproduce mask8800, accept the proven converted
upload, preserve native camera identity/frame gates and reject changes to each
of16 components. Boundary ABI, actual dispatch and actual culling rebuild PASS;
geometry/stereo union regressions PASS. This repairs the demonstrated activation
blocker; in-game CULL ON, peripheral visibility and90FPS remain NOT VERIFIED.

## 2026-10-28: recognize protected right-pad strokes after head turns

Protected slash directions were transformed by the fixed level/recenter yaw.
With head-following tentacles this is the wrong visual plane: after a90-degree
head turn, a visible left/right pad stroke becomes mostly depth in that old
frame and can be rejected. The actual-source x86 fixture reproduced the failure
before correction. Transform right-pad world velocity by the current tracked
head's inverse quaternion instead. This handles left/right and up/down strokes
after yaw, pitch and roll while preserving grip/rearm, threshold, pulse, pause,
focus and per-controller validity contracts. No native hook, targeting change,
bone adaptation or physics write. The legacy native-input path is unchanged.

Actual-body x86 fixture PASS671 checks:20 signed horizontal/vertical strokes
across five rotated/tilted head poses with the left pad untracked; stationary
pad/head-only motion and depth-only thrusts cannot slash. Existing focus/pause,
release/rearm, objective and reload checks pass. Profile isolation tests and
both builds PASS. Physical gesture acceptance and controller-directed left
pickup/throws remain unverified/unimplemented in the protected runtime path.

## 2026-10-28: separate grip motion from aiming-pose availability

LocateTrackedHands required both XR poses before locating either one. Thus an
inactive/failed/untracked aiming pose also discarded a tracked grip's velocity
and disabled protected right slashes and squeeze input. Locate each action
independently per controller, retain combined validity for existing native ray
consumers, and feed only grip validity to protected squeezes/right strokes.
Tracking/session resets clear all three validity states. No native writes.

The actual protected gesture body failed the new aim-loss stroke case before
the change and passes674 checks afterward. Actual LocateTrackedHands plus the
production TrackedHand/Motion run against independent XR action failures:
752 checks PASS across16 active-state combinations, both API failure kinds,
each missing tracking/validity flag, reacquisition and lost head views. Other
hand/actions remain live; grip reacquisition starts with zero velocity.
Protected XInput20041 checks and profile isolation2 tests PASS. Runtime and HMD
acceptance remain pending; this is input continuity, not a measured FPS gain.

Bounded read-only native follow-up confirms C7C710 (avatar virtual+2F4) returns
the camera smart-reference at avatar+62C; C66FF0 is a shared output-boolean
leaf, not a camera update boundary. C80910/98BB90 obtain controller references
through avatar+4E4 via6B0CD0, not a global singleton. These do not establish a
live owner publication route for a protected left-ray getter; no guessed
pointer scan, old EXE hook or private camera vptr is enabled. Private Ghidra
output remains outside the repository.

## 2026-10-28: selected-renderer player-camera ownership observation

Read-only Ghidra of the pinned EXE establishes a new candidate owner route:
985FB4 loads renderer+460, dereferences its smart reference, then985FCB calls
that object's virtual+2A0 for FOV. Avatar1001630 has494A40 in that slot; its
virtual+2F4 is C7C710, the verified camera smart-reference getter at+62C.
Earlier camera-step/view-update candidates offered no external owner boundary.
Private evidence: d2vr-selected-camera-world-source-ghidra.txt and
d2vr-player-camera-step-ghidra.txt in the user's local Temp directory.

At the existing mode3 owned985D80 callback, optionally inspect this exact field
and require the full avatar1001630 -> cameraFFC7C0 -> owner avatar reciprocal
plus avatar -> inventoryFFCA10 -> owner avatar reciprocal. Only a complete
match refreshes existing mod-owned gameplay feedback. No scanning, retained
native object ownership, new native calls, private camera vptr publication,
EXE/static-table/debug-register writes or native aim/throw/pose hook enablement.
A failed graph clears old feedback. Owner admission has independent loaded
getter/table pins; refusal preserves the visibility route. The existing cloned
985D80 full digest pins the field consumer before callback execution.

This is a read-only runtime corroboration path, not implemented controller
targeting. A future log must show Matched(status8) before using this route to
publish an aiming getter. Pure x86 graph regression4141 checks PASS: all links
missing/corrupt/null/unaligned, wrapped addresses, reciprocal-owner mismatch,
zero partial output and1000 successive owner substitutions. No game launch.

## 2026-10-28: protected per-controller left query ray

Diagnostics.ProtectedLeftAim is a new opt-in, default OFF. It requires the
protected image/input/gesture profile, head geometry and mode3 view boundary.
After the runtime owner graph actually matches, publish an immutable private
copy of the player's camera table (71 entries plus original RTTI/header),
changing only heap-instance slot9C. All native EXE/static/IAT bytes remain
unchanged. The mod module is pinned for the private table's process lifetime;
no instance pointer is retained for later destruction or restoration.

Read-only pinned EXE audit confirms C62F50 calls slot9C at C6300F and consumes
the returned matrix's forward/origin immediately at C63011/C63015, replacing
EAX at C63024 without retaining the pointer. The exact return address filters
the bridge; every other caller tail-calls original504810 unchanged. The known
caller gets the original leaf result once, then optionally a thread-local,
16-byte-aligned matrix. Registers except EAX, EFLAGS, complete x87/SSE state
and LastError are preserved. Nested calls use the native result. Admission
pins the entire702-byte consumer and entire original table/header by SHA256.

The controller ray is published only for tracked left aim while stock LB is
held, with triggers released and protected gameplay unpaused. Right-controller
tracking is irrelevant. Recenter reference orientation maps the ray into the
native camera basis; origin uses the configured GameUnitsPerMeter, matching
head translation/IPD. The native matrix stays untouched. Consumption requires
the same current camera/reciprocal owner, a sample younger than100ms and a
successful fresh gameplay capture. Character/retained-target branches keep
native targeting. Lock contention, invalid scale/tracking/reference/owner,
pause, trigger takeover and stale samples fall back to the original getter.

This changes the left ordinary-target query only. Swing-directed release
velocity is NOT implemented by this option. Legacy NativeLeftAim, native throw
EXE patching and other legacy input/pose hooks stay OFF. Physical aim/visual
alignment, private camera-table compatibility, CULL ON/peripheral visibility
and sustained90FPS require a future real run; no performance gain is claimed.

Offline x86: getter ABI/private-table fixture138015 checks PASS; actual-source
left policy459 checks PASS (including current branch refresh, right-pad loss,
TTL, five reference yaws and six world scales); owner graph4146 checks PASS.
Read-only actual EXE consumer/table contract PASS. Existing protected gesture
674 checks and controller/gesture profile tests PASS. Actual-source view phase
routing30000 cases and culling rebuild1000 native+1000 union cases PASS.
Production and diagnostic x86 builds PASS. Private disassembly stays outside
the repository. No game launch or HMD acceptance was performed.

Bounded next-step release audit (read-only pinned EXE, private Ghidra output):
D23220 is the fire table1004FE8 slot74 event method. Its ordinary held-object
branch calls A856B0 at D2396B, then878930 at D23972. D23220 spans0x775 bytes
through D23992 RET8; 878930 spans0x15BD through879EEC RET. Capstone and Ghidra
agree on every instruction boundary in both spans. There are41 and139 direct
calls, respectively; all jumps stay inside their own span. Neither span has a
computed jump, FS/GS operand or immediate self-code address. The fire table has
35 consecutive executable method entries. This narrows the candidate for a
private release-method copy using the existing owned-code relocator; it is not
yet an installed throw override or proof of full runtime compatibility.
Private evidence: d2vr-protected-throw-event-ghidra.txt. No game launch.

## 2026-10-28: protected left swing/aim release implementation

Diagnostics.ProtectedLeftThrow defaults OFF and requires ProtectedLeftAim and
its full protected-input/mode3/head-geometry admission. The active owner route
now admits only a live camera -> avatar -> inventory -> left demon weapon ->
current-mode fire -> weapon reciprocal. It publishes a private35-method fire
table at heap-instance slot74 after pinning the entire source table/header and
both audited native methods by SHA256. Existing owned-code relocation copies
D23220/878930 and relocates all180 E8 calls, linking the copied event to copied
release. Only the copied879C88 comparison call is redirected. Native EXE,
static tables, imports, syscall path and debug registers are never edited.

The comparison adapter preserves GPRs/EFLAGS and x87/SSE/MXCSR, invokes a
guarded velocity resolver, restores state, then tail-calls original6DA050 once
with its original stack arguments. The existing one-shot650ms native throw
token now supports this independent private route. Live owner, weapon mode,
fire, exact attached entity and ordinary-prop branch are revalidated before
changing XYZ; magnitude and W remain native. Character/retained-target paths,
spin and downstream physics calls are unchanged. Source functions retain a
strict default legacy path; the protected feature never enables their EXE
installers.

A separate left-hand recognizer uses the existing powered-stroke selection and
ThrowDirection blending. It supplies stock acquisition/release LB pulses with
a neutral gap. Sustained held-grip aiming remains active between pulses. The
right recognizer/stick are independent. Trigger ownership, tracking/reference
loss, pause, invalid frame context and a changed camera/inventory/fire owner
cancel or rearm the gesture; a new owner cannot inherit its predecessor's
stroke. Release direction uses the stroke's left-controller velocity/aim in
the recenter reference basis, never current head gaze.

Offline evidence: pinned full CFG/metadata audit PASS (2 blocks,180 calls,35
table methods/header; no escaping/indirect jumps, local SEH or code pointers).
Adapter own-DLL x86 fixture compares2000 native-vs-adapter invocations, including
EDI fire receiver, argument/return/stack, GPR/EFLAGS, x87/SSE/MXCSR, LastError and
the native comparison's immediate vector read. Actual-source throw fixture162
checks PASS: live owner/entity/alternate rejection, token expiry/one-shot use,
native magnitude/W, powered stroke plus controller aim, neutral LB gap,
right-pad loss/unchanged right inputs, pause and owner-change cancellation.
World direction is normalized after the camera-basis transform, preserving
native speed even for an admitted slightly imperfect orthonormal basis.
Actual-source left aiming461 and right gesture674 checks PASS. These are
offline contracts, not game/HMD/FPS acceptance. Native callees see relocated
return addresses; runtime compatibility and final physical trajectory remain
unverified. Peripheral visibility and sustained90FPS are still unproven.

## 2026-10-28: independent protected objective/reload input

The protected gesture path previously cancelled the shared objective/reload
pulse when either controller lost tracking, and required an aiming pose for
grip-position dwell. The actual-source fixture reproduced this at check634
before the fix. Protected reload pulses now retain their emitting hand in
separate deadlines. Each hand cancels only its own pulse when its grip/context
becomes invalid; objective belongs to the left grip. Aim-only tracking loss
does not invalidate grip dwell. Existing pause/focus/rearm and legacy routing
remain covered. Actual-source x86 fixture:734 checks PASS. Production and
diagnostic x86 builds PASS. No HMD or performance improvement is claimed.

A private test of the existing stereo-frustum fixture using the native .95
depth reconstruction before building headClip passed240 tracked poses and
249600 binocular points. This did not reproduce peripheral clipping and does
not justify another projection change. Latest available runtime log remains
the16:14 run preceding the installed shader-depth comparison correction;
current CULL activation and peripheral visibility remain NOT VERIFIED.

## 2026-10-28: retain preceding-frame CULL admission during current uploads

The selected-camera callback requires the immediately preceding frame's fully
corroborated camera/projection before it can widen the native frustum. A later
matching upload in the current frame replaced that receipt. Another native
copy in the same XR frame then failed admission even though the independent
preceding-frame proof was still valid. Reproduced in the x86 boundary fixture:
FAIL line90 CanCoverProjection(101, same view/renderer/camera/native inverse)
when Begin corroborates an already-observed frame101 upload after frame100.

ProjectionWitness now retains one preceding verified receipt when advancing
the latest verified frame. Repeated current-frame uploads do not overwrite
that receipt. Admission still requires a receipt exactly one frame old, exact
view/renderer/camera identity and the complete native inverse; same-frame-only
proof, projection changes and stale frames remain rejected. No render matrix,
native image bytes, driver API or frustum construction algorithm changes.

Offline PASS: existing x86 external-boundary ABI/concurrency fixture plus the
extracted production ObserveNativeSelectedCamera body for repeated copies,
early uploads, unknown shader projection, full-matrix mismatch, camera change,
missed frames and prepare/rebuild failure. Geometry fixture PASS:240 tracked
poses,249600 binocular points,201581 outside the original cone. Actual-source
rebuild ABI fixture PASS:1000 native-route and1000 conservative-route calls;
this fixture stubs native algorithms. Production and diagnostic x86 builds
PASS. This is a reproduced admission bug, not proof that repeated callbacks
caused the user's latest CULL OFF. The available runtime log still predates
the installed depth fix. Current HMD visibility, gesture trajectory and90FPS
remain NOT VERIFIED; no game was launched.

## 2026-10-28: restore stock Y for held-target actions

ReadVRGamepad mapped the left upper face button exclusively to Start/recenter,
leaving no XR route to stock Y. Gameplay left-grip + triangle now emits Y;
ContextTriangle latches action versus menu ownership at the button press.
Grip changes cannot turn a held Y into pause/recenter or a menu hold into Y.
Session loss, polling gaps, nonfinite/reversed time and startup-held input
require release/rearm. Menu-button Start and the remaining face buttons retain
their mapping. LocateTrackedHands runs once, before triangle selection, so the
modifier uses the current synchronized grip rather than a previous-frame pose.
The native engine selects its own contextual action; no execution function,
target, EXE bytes or gameplay state is overridden.

XInput fixture production97 / diagnostic151 checks PASS, including held Y,
grip-release ownership, menu/recenter ownership, focus/time loss and hold option
OFF. Both x86 DLL builds PASS. Physical opponent actions remain NOT VERIFIED.

New authoritative runtime log (18:29:38,355464 bytes) contains CULL applied
receipts and native source2676x2728. User reports no disappearing elements when
CULL ON. It also exposes rejected player ownership: renderer+460 is absent in
normal gameplay (status0), and no private left camera/fire publication occurs.
Thus the previously implemented native left aim/throw paths did not activate
in this run. Renderer+460 is an optional native FOV override, not a proven
always-present local-player source. Keep strict admission; investigate a
correct native owner producer rather than bypassing the failed edge.

Some GPUFrame batches report D3D9 live+replay11.787/13.993ms, excluding D3D11/XR,
already over the11.111ms90Hz budget. These mixed/toggled scene batches are not
a matched A/B and do not identify the expensive pass or prove a CPU saving.
Current high-resolution90FPS acceptance remains open.

## 2026-10-28: identify current-frame CULL refusals and missing player route

User reports CULL OFF again. The available log still ends18:29:38 and cannot
identify this new refusal. Existing bounded boundary reports now expose the
pre-Begin witness decision, shader-upload agreement, boundary readiness and
prepare/rebuild outcome; route3 camera-contract rejection is also bounded.
No culling admission, geometry, indicator semantics or performance path changes.

Read-only renderer rejection metadata additionally records the pinned+3F8
context reference, its native virtual+CC getter, selected render camera and
optional+460 override. Ghidra985D80/D05F20 confirms these fields are consumed
by the native selected-view path. Unknown getters are never called. The missing
current-view player owner remains unresolved; native left aim/throw is not
claimed active. Private decompilation remains outside the repository.

Read-only owner regression4162 checks PASS. Actual-source selected-camera
admission and x86 boundary ABI regressions PASS; production and diagnostic x86
builds PASS (diagnostic rebuilt after CULL logging). No game launch or runtime
performance/visibility fix is claimed by this diagnostic candidate.

## 2026-10-28: distinguish D2Avatar from LobbyAvatar before owner admission

Verified D46 image constructor4AB030 publishes1001630. Derived constructor
ABD6C0 subsequently publishes100C918; both tables share C7C710 at+2F4 and
494A40 at+2A0. Native descriptor initializers identify110F5D8 as D2Avatar
and1116B90 as LobbyAvatar. The latter must not be admitted merely because its
camera/FOV getters match. No type checks were weakened. C5CC40 initializes
the10DE3F8 manager's strong references to the native null sentinel, providing
no always-present avatar. Functions9A08E0/55AFF0 copy unrelated470-byte array
elements; their+460 operands are not a selected-renderer owner setter.

Read-only Ghidra outputs completed with matching EXE SHA and remain private.
The runtime log remains355464 bytes, last written18:29:38; it predates bd6d82a
and contains no new context/getter rejection metadata. No current-view player
source is established by this static pass. Do not change native aim/throw
admission until the selected context's actual getter can be identified.

## 2026-10-28: observe the selected view's already-live native reference

Followed native GetPlayerAvatar/GetLocalPlayer binding records to B73EE0,
964570 and43BCB0. Confirmed RegionMgr base tableFC7D00, player vector+AC/+B0,
and Player+74 avatar reference. These do not yet establish renderer+3F8's
concrete region getter; no guessed RegionMgr traversal is installed.

Instead, the existing private985D80 copy now observes the result of its native
virtual+174 call at985F75, while the caller's out-reference remains live.
The adapter forwards the original thiscall(out) exactly once; captures no new
native reference; preserves SIMD/x87, native return/out pointer and LastError;
then replays MOV ECX,[EAX] and CMP [ECX],0 with original flags. The seven-byte
whole-instruction replacement exists only in owned, digest-pinned code before
RX sealing. Static CFG independently verifies instruction boundaries and no
incoming branch into the displaced span. Relative-call relocation overlap
refuses. Native EXE/static vtables and debug registers remain untouched.

The callback admits only the original selected renderer/camera context and
complete D2Avatar1001630 -> cameraFFC7C0/private -> avatar and inventoryFFCA10
-> avatar reciprocal graph. A valid live result can prepare existing private
left aim/throw paths without the absent renderer+460 override. The later
frustum callback preserves this receipt only for the exact frame/renderer and
fresh matching gameplay camera. Invalid results retain strict fallback.
Actual return type/acceptance in gameplay is NOT established offline.

Read-only owner graph4167 checks PASS; actual-source live-reference callback54
checks PASS; adapter1000 native calls/observations PASS for empty/nonempty
references, output/CMP flags, stack, floating state and LastError; existing
10000-callback clone checks PASS. Full pinned CFG PASS461+1110 instructions,
131 direct calls. Existing CULL boundary and protected throw162 checks PASS.
Both x86 builds PASS. No FPS gain, physical controller trajectory or independent
tracked visual hand/bone pose completion is claimed. Enemy face-button mapping
from cee43ef remains available independently of native-owner activation.

## 2026-10-28: direct Y access during native-confirmed opponent holds

The prop-throw recognizer owns LB and suppresses its pulses during alternate
character/retained-target holds. Requiring another L1 press merely to select Y
couples contextual action access to that separate native grip/release button.
NativeHeldActionContext now allows triangle to select stock Y directly during
a fresh alternate hold belonging to the admitted private camera. The existing
ContextTriangle latch prevents disappearance of that feedback during a held Y
from turning it into Start/recenter. Neither controller pose is needed for a
face-button action. Outside this context, the prior L1 modifier/menu mapping
remains. Native LB, native enemy action selection and prop physics are unchanged.

Actual-source protected throw/context fixture171 checks PASS; existing XInput
bridge production97/diagnostic151 checks PASS, including latched action/menu
ownership and input/time loss. Both x86 builds PASS. Native-owner activation
and physical opponent execution still require runtime evidence; no FPS claim.

## 2026-10-28: identify the CULL OFF admission failure

User reports CULL OFF with peripheral disappearance. Current installed INI
already requests NativeViewBoundary=3; the retained log ends18:29:38 and
predates the installed2805abf candidate. It cannot identify the reported run's
failed admission. No culling acceptance, geometry fix or FPS gain is inferred.

The existing current-frame status now appends the actual refusal stage:
1 boundary not reached,2 frame/gameplay context,3 selected camera contract,
4 native projection,5 preceding-frame projection proof,6 live shader match,
7 binocular geometry preparation,8 frustum rebuild. A successful rebuild is
still the only source of CULL ON. Each newly encountered reason is logged once,
with bounded600-frame refreshes, in NativeCullingStatus. Counters reset at the
XR frame boundary; no preceding-frame success is displayed as current success.
Native admission and all memory-write conditions remain unchanged.

Actual selected-camera x86 admission/ABI regression PASS. Actual final pixel
shader WARP regression PASS for CPU/CULL combinations and all200 pixels of
the eight refusal digits. Production and diagnostic x86 builds PASS. Game
not launched; the cause of this user's OFF state still requires a fresh run.

## 2026-10-28: independent grip poses at the private avatar skin packer

User clarified that CULL ON appears after several seconds and peripheral
visibility then works. Treat that as their visibility acceptance; do not spend
further work on the initial OFF interval without a new visibility regression.

ProtectedTrackedHands is a separate, default-OFF diagnostic candidate. The
selected-view live avatar reference can publish a private copy of the resource
FC2578 table,96 methods plus its preceding word. Only +EC is redirected;
the original6C3890 thiscall(rendererRef) is forwarded exactly once and retains
its native smart-reference release and all side effects. Native constructor
AEDED0 and destructor541620 establish the concrete table; its next dword at
FC26F8 is scalar data. Full table/header and6C3890..6C3EDB digests are pinned.
Destruction retains the original first entry6905B0 ->541620, which restores
the native vptr. No retained instance is dereferenced at shutdown.

Read-only D46 Ghidra exports private-hand-packer.txt and private-hand-resource.txt
both end EVIDENCE COMPLETE; their SHA256 values are
24bd6771e5f8b0e10c544ee7217c8222e11e82d685870c4910f21de451e9d4f3 and
7b7516a8abd0808b523a3feaa5f19a71e54a320d15ba3cb704ab55b23a64ffd8.
Raw exports remain outside Git. Existing remap/GPU-consumption uncertainty
remains: a CPU palette write is not proof of a visible human-hand correction.

New grip-space samples are independent of legacy pistol hooks, aim-space
tracking, optional support snapping and the peer controller. Fresh samples are
camera-owned; recenter/reset clears them. The actual guarded IK callback now
allows either arm to proceed when its peer loses tracking or cannot solve.
Only solved human-arm ranges are committed; camera/tentacle bones and unsolved
peer ranges remain unchanged. Full avatar/model/renderer/resource/skeleton
identity is checked at the pack boundary. Busy/unknown/alternate native holds
keep stock animation, leaving enemy actions and existing left throw untouched.
Matched capture explicitly records the new policy and no longer labels such
a candidate native-hooks-suppressed.

Actual full-source protected hand fixture1088 checks PASS, including all six
pose/skin slot combinations, repeated-pack drift, one-hand loss/invalid pose/
failed IK, native thiscall/ref-count-once forwarding, x87/SSE/LastError,
recursive bypass, hold/type/context refusal and all95 unchanged table entries.
Native geometry ten seeds502400 cases/43368640 checks PASS; owner graph4167
and actual selected-view callback57 checks PASS; protected throw/context171
checks PASS. Production/diagnostic x86 builds PASS. Native owner activation,
visible model alignment, opponent execution and90FPS remain unverified.

## 2026-10-28: remove system copies for fresh active native caller-stack buffers

Continued the measured RecPush -> ReadProcessMemory -> WOW64 copy path rather
than reopening resource-demand caching. The retained D46 forwarder contracts
B74720/96E3B0 already pin the native API return and three duplicated arguments.
Their synchronous source lifetime also covers an outer active caller's local
buffer; the existing 571670/AD3380 EBP allocation check excluded these sources.
InspectCurrent now tries a separate fresh-TIB stack-range receipt after the
original receipt rejects. No heap/EBX ownership is inferred or remembered.
The full source must lie above the incoming native stack and below this
thread's current StackBase, with exact source/count/first-register duplication
and the original image/feature/fault admission. Other threads and heap sources
retain baseline copying. Unexpected copy faults still latch admission OFF.
Manual protection of otherwise active stack pages remains outside the native
readable-buffer contract, as for the previous compiler-owned local proof.

Actual production inspector + extracted PS hook x86 test7104 checks PASS,
including outer locals with and without EBP, exact nonzero payload equality,
thread/heap rejection, fresh bounds, both register-bank limits, all existing
material-field refusals and guard/fault regressions. Actual RecPush/small copy
6096 checks PASS. Production and diagnostic x86 DLL builds PASS. Synthetic
64-byte capture/stub forwarding measured647.5ns RPM versus41.0ns active stack;
not a game saving. Separate activeStackProved/activeStackCopies receipts expose
incremental runtime coverage. No game launch or90FPS proof; original resolution,
hands, throw, opponent action and culling paths are retained.

## 2026-10-29: protected controller direction for native character throws

Pinned release879C88 shares its velocity with character consumer75AD20 via
879CA6; saved ESI is the retained +584 reference, alive through that call.
75AD20 forwards the same pointer/mode2 to the character body array at+4F0/+4F4.
The protected comparison adapter now carries ESI and admits a freshly owned,
unchanged character target; +590 and legacy character overrides stay excluded.
Native speed/W, spin, animation, reference ownership and body fanout are retained.
Private Ghidra output: TEMP/d2vr-character-throw-75ad20.txt. Source/ABI/native
consumer regressions pass; full enemy lifecycle and HMD acceptance remain open.

## 2026-10-29: keep tracked wrists attached through fully folded arm poses

PoseArm previously rejected controller targets within abs(upper-lower)+1mm,
including every target less than5mm from the shoulder. Valid tracking could
therefore leave the arm at its previous/native pose. The analytical solver now
translates the shoulder by the minimum distance to the inner reach boundary,
as it already did for the outer boundary, while retaining the exact wrist pose
and both rigid segment lengths. A coincident target uses the native arm axis;
root, peer arm and demon/attachment bones remain outside the edit scope.

The new regression fails on the preceding solver. Ten x86 randomized seeds
pass50240 geometry cases each, including either segment being longer, exact
shoulder targets, rigid lengths, wrist orientation and untouched other bones.
Actual protected packer regression58811 checks and private native B1E500 skin
arithmetic pass. This proves CPU geometry and packing, not HMD appearance or
the cause of every reported hand interruption. No performance saving claimed.

## 2026-10-29: pre-scene clears and derived equipped Gun admission

User acceptance narrows remaining flicker to menus/pause and reports an upward,
unresponsive left pistol in dual wield. Retained d3d9.log contains a protected
publication for slot0 only, not proof of the left gun's concrete rejected type.

Clear is legal before BeginScene. The recorder previously started only at
BeginScene, losing those clears; right replay could retain prior history while
the live eye had cleared its target. A pending frame now starts at the first
native render-thread Clear or BeginScene. Clear captures pre-state before
forwarding and remains in the same stream at BeginScene. Presentation internals,
other threads and other submission modes cannot open recording. A failed
pre-clear snapshot latches overflow even if BeginScene later captures state.
Actual-source x86 fixture9009 checks/1000 sequences passes; confirmation that
this explains the user's menu flicker remains a runtime acceptance item.

Read-only Ghidra export TEMP/d2vr-gun-type-contract.txt confirms native
CFBBE4 tests IsA(1119658) and4BF720 follows instance+4, descriptor+0C. Protected
weapon ownership now accepts this Gun lineage with a bounded, SEH-guarded
walk, rather than requiring only concrete1010E30. Exact pinned model/fire
classes and all reciprocal equipped ownership checks remain required. This
fixes a valid derived-gun exclusion; the actual left gun's type is unconfirmed.
A receipt once per changed rejected equipped identity records class/type,
model and fire tables without a per-frame log stream. No angle bias or unsafe
receiver admission was added. Native EXE/static tables and input routing stay
unchanged. HMD aiming and menu flicker acceptance remain open.

## 2026-10-29: controller shot matrix after native camera replacement

Actual CFBBA0 ignores its incoming controller matrix on the camera-enabled
branch: CFBC38..CFBD15 obtains native camera/avatar vectors and75C320 builds
a new matrix, later passed by CFBD60 to C47570. The previous entry-only
adapter could therefore pass all ABI tests yet fail to steer that branch.
Ghidra exports TEMP/d2vr-left-weapon-contract.txt and
TEMP/d2vr-shot-consumer-contract.txt confirm the data flow and full consumer.

The equipped fire instance now publishes an immutable full CFBBA0 copy with
all11 relative calls relocated and only CFBD60 redirected to the existing
controller adapter. It calls original C47570 once after fresh ownership/pose
validation. Native reference/query/no-shot/cleanup paths are preserved; no
EXE, static table, ammo, animation or input mapping changes. Both full native
fire and C47570 bodies are SHA-pinned before publication. Model code unchanged.

The fixture executes the exact private551-byte CFBBA0 body3200 times against
owned callees. It reproduces camera replacement of an already-correct incoming
controller matrix, then checks final controller matrices for each hand,
camera/fallback/no-shot branches, reference balances, unchanged five arguments
and outgoing AL. Three absolute data operands and11 external calls are
relocated only inside fixture-owned memory; original bytes stay outside Git.
This strengthens CPU consumer evidence beyond the previous stub-only ABI test.
Physical gun alignment, exact left gun admission and90FPS remain unverified.

## 2026-10-29: retain pre-scene surface copies and fills

The pre-scene capture fix covered Clear only. Existing ColorFill and
StretchRect hooks still recorded solely when a packet was already active,
so a copy/fill before the first BeginScene was forwarded live and omitted
from right-eye replay. Both hooks now use the same guarded packet opener
before their native call. It captures once, keeps command/rectangle ordering,
and retains the capture-failure refusal through BeginScene. Shared texture
uploads, replay execution, frame boundaries and synchronization are unchanged.

The actual-hook x86 fixture passes 9129 checks, including full and rectangular
copies/fills, mixed write order, exactly one capture, forwarding, render-thread
and presentation gates, and capture failure. This proves a missing-command
case; retained game evidence does not establish that this case caused the
user's menu flicker. No headset or frame-time improvement is claimed.

## 2026-10-29: reject packets after failed live state/surface writes

Twenty record-before-forward hooks previously retained commands even when
the original D3D9 call returned a failure. A replay could then successfully
perform a state change, clear, fill or copy that the live eye never performed.
RecordLiveResult now preserves the exact native HRESULT and marks an active
packet incomplete on failure. Later successes cannot repair that packet;
existing replay/history/layer refusal handles it. Success adds no lock,
allocation, retry, cache or driver call. API arguments and command order stay
unchanged. This covers the twenty hooks listed by build_live_record_status;
it is not a claim that every live native error path is now transactional.

The actual-hook x86 fixture passes 2405 checks across twenty APIs, five
HRESULTs, recording ON/OFF and prior incomplete/complete packets. It checks
every original argument, exactly one driver entry, exact returned HRESULT,
failure-only locking and sticky refusal after a subsequent successful call.
Pre-scene writes pass 9129 checks; actual replay surface and state receipt
regressions also pass. These are CPU fault-injection fixtures, not evidence
that this error occurs in the user's menu. No performance saving is claimed.

## 2026-10-29: independent shared inputs for same-frame stereo

The deferred transfer worker previously reused one D3D9Ex/D3D11 shared input
for both eyes. Submitting the right copy therefore waited for the left GPU
reader even after reclaiming the CPU context lease. The worker route now uses
one input per eye, each with its own D3D9 producer query, D3D11 reader query,
fence identity and retained packet. Only the prior reader of the selected
input can block its next overwrite. Both images still come from the current
application frame; simulation, tracking, stereo replay and XR sequencing stay
unchanged. This removes a dependency, not a synchronization proof.

Two slots require the existing worker, deferred-reader and last-consumer-fence
options. Serial and nondeferred modes retain one slot. Extra allocation failure
keeps the original fenced route and is not retried until Reset; it cannot accrue
primary capture failures. Resize drains the context lease and retires that
slot. Global teardown verifies both readers before freeing either resource
lifetime; failed reads retain resources and refuse reuse/Reset. Legacy untracked
readers are also fenced at resize/teardown. The extra input uses 29,200,512 bytes
(about 27.85 MiB) at 2676x2728, excluding driver metadata. Copy count is unchanged.

Actual owner/packet-building code passes 22,908 x86 CPU checks, including 1000
two-eye sequences, independent source/query identities, exact packet selection
in worker modes 1/3, original one-slot routing, own-reader refusal, retained jobs,
all seven creation failures, no repeated optional allocation, failed queue
publication, resize and legacy teardown. Existing reader-fence protocol passes
2000 worker jobs. Both DLL variants build. The real x86 D3D9Ex/D3D11 interop
fixture passes 2000 eye transfers, including 1000 at 2676x2728 and both new
two-input scenarios. It checks sampled pixel equality, reader retirement,
concurrent D3D9 work, joins, failure refusal and Reset/recreation. No game or
OpenXR calls occur. Game CPU/GPU savings, latency, HMD image correctness and
stable 90 FPS remain unproven.

## 2026-10-29: separate physical refresh from application pacing

The first adaptive-FPS prerequisite now reads physical Hz only through an
advertised and enabled XR_FB_display_refresh_rate extension. Instance startup
falls back to the original D3D11-only extension list if optional admission
fails. A successful session query establishes the rate; refresh-change events
update it. Stopped/lost sessions and invalid results invalidate it. Unsupported
runtimes report zero/unknown, never an assumed 90 Hz or the inverse of an
already throttled predictedDisplayPeriod. Discovery is bounded; queries occur
on session start, with events thereafter, rather than on every frame.

The actual adapter passes 360 x86 checks for discovery/list growth, unsupported
functions, failed queries, session-loss results, fractional Hz, rate changes
and stopped-session invalidation. This is a prerequisite, not an implemented
OFF/AUTO/HALF menu or runtime half-rate control. No frame sleeps, dropped stereo
eyes, display-rate requests or SteamVR settings are introduced.

API contract: https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrGetDisplayRefreshRateFB.html
and https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrEventDataDisplayRefreshRateChangedFB.html

## 2026-10-29: secondary shot without a primary-model reference

The new real-game log publishes the right gun's private shot route but refuses
the secondary gun: its +12C model reference is empty, while its fire table and
type are the pinned 1010AE0 / 11193F0. WeaponOwner incorrectly made that optional
model a prerequisite for admitting the separately owned firing action. Resolve
also required the model to have a private table before applying any shot ray.
Both gates now distinguish model and shot ownership. Slot identity, reciprocal
avatar/gun/action references, selected fire mode/type, tracking and native
consumer contracts remain required. Unknown models are never published. An
empty model cannot block an otherwise proven equipped shot action. Repeated
preparation of an already owned fire-only gun does not republish or log per frame.

Actual-source x86 regressions pass 121991 checks including absent, unpublished
and foreign models, independent left/right rays with peer tracking lost, wrong
slot/receiver refusal, and the actual final-consumer bridge with an absent
model. Existing 3200 copied native CFBBA0 executions and ABI comparisons pass.
Positive initial publication of a previously stock fire action remains a game
validation item; fixtures do not load the game's live instance tables.

## 2026-10-29: native world-space tentacles throughout executions

User feedback distinguishes correct ordinary holds/prop pickup from separation
during an action on a held enemy. Installed Render.HeadAttachedTentacles=1
cancels the world head transform only on tentacle draws whenever sampled grab
references are Empty. Those references do not prove that a native execution
has finished owning the victim. Both current candidate profiles now use the
existing supported HeadAttachedTentacles=0 path, preserving native world-space
tentacle transforms for the entire sequence, including reference handoff.
No enemy transform, physics, animation, controller input or execution timer is
modified. Idle tentacles also retain their native world-space transform; headset
comfort and action alignment require user validation. The legacy experimental
head-attached profile remains available. Offline tests cover both eyes and live
draws across 30 head poses, 3 shaders and 7 mesh offsets, with Empty/Busy feedback:
3780 disabled scopes preserve the exact world matrix without device queries.

## 2026-10-29: menu pointer geometry, capture and navigation core

Added a renderer-supplied menu-surface contract and real 6DoF ray intersection.
The caller must supply the actual submitted surface, native cursor rectangle,
reference space and application frame; no guessed plane or gameplay heuristic
is admitted. Pixel mapping includes edges and translated/rotated controllers.
Trigger hysteresis acquires only inside the menu, keeps slider capture outside,
and cancels once on tracking/context loss. A held trigger cannot click on
reacquisition. Back is edge-triggered and works without aiming at the plane;
scroll uses application time with a deadzone, reversal reset and no resume burst.
Native integration must merge physical mouse ownership and avoid duplicating
the existing gamepad Back route. These headers are not yet included by the DLL.

The x86 /W4 /WX regression passes 181360 checks: independent Rodrigues/target-ray
geometry oracle, 20000 6DoF cases, 10000 click cycles, drag/context cancellation,
held Back on reacquisition, and equal-duration scrolling at 36/45/60/72/90/120
application FPS. Native UI, cursor, laser rendering and HMD validation are NOT RUN.
No Steam deployment or performance improvement follows from this component.

Retained SHA-pinned native evidence identifies mouse update 585450 and generic
device update 9A5D90. The latter forwards DirectInput GetDeviceState at 9A5E3A
and GetDeviceData at 9A5E6E, with 20-byte event records. The +7 byte is set after
Acquire and cleared on failed reads; it is not a proven menu-mode flag. The
ScreenToClient caller 5854C6 writes the native client cursor at +21C/+220.
Lua IsPauseMenuShowing wrappers DA5FF0/DD2100 call a typed receiver's +80 virtual
method; neither wrapper establishes a global receiver. The Flash MouseEvent
string consumers 44BFF0/78D400 initialize ActionScript values, not a proven
physical input delivery path. Do not use these references as menu-state or
click-injection contracts. Private disassembly remains outside Git.

## 2026-10-29: distinguish equipped-fire publication from shot consumption

Headset feedback rejects the independent-left-shot fix: the left pistol still
fires along the right pistol's aim. The retained runtime log does publish the
secondary fire receiver, so optional-model admission alone was insufficient.
Add at most two receipts per final-consumer outcome per thread, identifying
context, sample, owner, equipped receiver, native gun argument, tracking,
transform refusal, or accepted right/left ray. No admission rule, native
reference lifetime, matrix selection or render setting is changed. This is a
bounded diagnostic candidate, not a corrected-shot or FPS claim.

Actual-source x86 regression: 122019 checks; 3200 copied native CFBBA0 executions
and existing ABI/FP/LastError comparisons pass. New tests distinguish a closed
XR frame, wrong gun argument and missing active-hand tracking from an accepted
left/right ray, retaining no-write behavior on refusal. Physical validation
remains failed for the prior candidate and pending for this diagnostic.

## 2026-10-29: fail closed after rejected live draws

The four DrawPrimitive/DrawIndexedPrimitive/UP hooks recorded before forwarding
but did not mark the packet incomplete on a failed native draw. They now feed
their unchanged HRESULT through RecordLiveResult before clip restoration.
Successful draws add no driver calls or failure-path lock. Deliberate diagnostic
draw suppression still returns D3D_OK. Replay's separate draw-error checks remain.

The actual-source x86 regression passes 2848 checks over 24 APIs and 20 status/
recording combinations, including all four draw variants, exact arguments,
one native call, UP recording ownership, clip cleanup and sticky invalidation.
This closes a proven publication gap; no runtime draw failure, yellow-artifact
root cause or performance improvement has been demonstrated by this fixture.

## 2026-10-29: adaptive policy and actual presentation receipts

Add the OFF/AUTO/HALF policy and opt-in INI integration. OFF is the default.
Only a fresh stereo XR_SUCCESS submission paired with a forwarded native
D3D_OK Present can count as an application frame. Nested/aborted presentations,
empty layers, stale pair IDs, background sessions and positive non-success
statuses cannot count. The callback preserves LastError and live x87/MXCSR.

The policy covers actual 72/90/120-family refresh rates, sustained low unique
application cadence, hysteresis, release failures and recovery from complete,
fresh CPU/GPU workloads independently of capped FPS. No pacing adapter is bound
and complete workload feedback is not connected: AUTO/HALF currently report
UNSUPPORTED. No sleeps, skipped eyes, refresh-setting changes or compositor
control are introduced. The VR Settings UI and real runtime transitions remain
unfinished; this is not a completed Adaptive FPS feature.

Validation: 3243 x86 policy/receipt checks, 1057 actual-source host-integration
checks with native exit 0, and 360 physical-refresh adapter checks pass. Both
production and DiagnosticCapture x86 DLL builds pass. The integration fixture
needs an explicit main return with its inline assembly. Bitdefender held its
new EXE during scanning; the harness retries only sharing violation 32 on the
same file for up to 40 seconds and never treats a failed start as a pass.
No game, headset, runtime pacing or FPS improvement is claimed.

## 2026-10-29: reject recorded pairs after failed live pixel constants

Native_SetPSConstF now applies the same sticky live-submission failure rule as
the other recorded setters and draws. Previously a failed native pixel-float
upload could remain in an apparently complete recording and succeed only in
the right eye. Preserve the exact driver HRESULT, one forward call and upload
bits; successful calls add no result lock or driver calls. This closes a
publication correctness gap, not a proven cause of the reported flicker.

Validation: 9339 actual-source x86 ownership/ABI/failure checks pass, including
20 combinations of recording state, existing invalidation and driver status.
Both production and DiagnosticCapture x86 DLL builds pass. No runtime FPS or
headset improvement is claimed; the installed candidate is unchanged.

## 2026-10-29: keep an equipped pistol's human socket in shot aim space

Fresh installed-83fdb07 receipts accept both right (stage 32) and left (33)
controller matrices. The user confirms independent left hits but reports an
upward, misaligned visible barrel. The secondary fire action is published with
an empty optional +12C model. CopyProtectedWeaponHandPose previously required
that model to be published too, so this exact case fell back to controller
grip while its shot used aim. Select aim for the already-owned human palette
from the reciprocal equipped gun/fire contract independently of the optional
model. No unknown model is admitted or written, and the shot path is unchanged.

Validation: 122025 actual-source weapon checks (including absent/unpublished/
unknown optional models with independent rotated aim), 3200 native-body fire
fixture executions, and 2207 actual protected-hands checks pass. Visual barrel
alignment and remaining animation/socket offsets still require headset proof.

## 2026-10-29: preserve OpenXR swapchain image ownership across failed waits

WriteTextureToSwapchain treated any nonnegative wait result as write permission
(including XR_TIMEOUT_EXPIRED), and released even after a failed wait. Keep one
acquisition per eye until its wait succeeds without timeout; retry that wait
without a second acquire. A release failure retains the waited acquisition and
retries release without another wait. Session-loss cleanup is balanced but does
not qualify a submitted pair. Validate image indices, texture arrays and context
before copying. Successful swapchain destruction resets the lease; failed
destruction cannot erase it. Normal writes retain the same A/W/copy/R call count.

Basis: Khronos OpenXR-Docs rendering.adoc (xrWaitSwapchainImage and
xrReleaseSwapchainImage), and hello_xr/openxr_program.cpp lifecycle comparison:
https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/rendering.adoc
https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/openxr_program.cpp
No external implementation code copied. UEVR's inspected OpenXR implementation
keeps pipeline frame metadata and explicit begin/sync state, but its repository
LICENSE is all-rights-reserved: no code imported. Its Unreal integration does
not directly replace this D3D9 recorder or establish a SteamVR OpenXR half-rate API.

Validation: 12129 actual-source x86 checks, including 2000 alternating eye
writes, timeout and error retries, independent eye leases, release retries,
session-loss statuses, bounds and reset. Production and DiagnosticCapture x86
DLL builds pass. No timeout/failure occurrence in the user's session, flicker
root cause, runtime performance saving or headset acceptance is claimed.

## 2026-10-29: close the original hands immediately behind their wrists

The user accepts af9cdd7's independent aim and barrel alignment. Preserve that
installed DLL. The previous sleeve-only asset retained exposed forearms and an
open wrist. Clip the SHA-pinned original hand material at the actual inverse-bind
wrist joints, with a 5 mm cuff toward each elbow, and cap each cut contour in all
five LODs. Preserve original textures, kept vertices, skeleton, buffer sizes and
native draw bounds. Reuse removed forearm vertex/index slots; no runtime CPU
clipping, new draw or shader is introduced. Do not distribute extracted assets.

Validation: three private stock-geometry tests pass, including closed LOD0 mesh,
closed cut contours in every LOD, valid skin weights, exact retained vertices,
unchanged skeleton/layout, original draw bounds and deterministic preimage
refusal. Eight generated transaction tests pass, including sleeve-only upgrade,
exact rollback to that previous candidate, interrupted append and foreign-data
refusal. Lower stock LODs retain their existing simplification holes away from
the wrist; this change closes the new cut, not unrelated stock topology.

Installed with fresh elevated game-closed CIM check and recovery receipt in
backups/closed-wrist-20261029T150240 under the private runtime root. Prepared
payload SHA256: 71781234a1851232a74876504ccda0d8a86365dc358efcefc11d79454b47a936.
This is an asset-only update; no C++ rebuild or new x86 result is claimed.
Headset appearance and articulation require the user's next test.

## 2026-10-29: reject restored shader state when binding queries fail

SyncActiveShader cleared stale metadata on failed GetVertexShader/GetPixelShader,
but returned no status. A snapshot, replayed state-block Apply or native live
Apply could therefore publish a pair with unknown shader interpretation. Return
the combined query status and propagate it through all three callers. Valid null
fixed-function bindings remain accepted. Keep successful call counts, native
HRESULTs, shader references and cleanup order unchanged; no query retry added.

Before correction, the actual c1a0985 live hook fails the new x86 assertion at
check232071: failed binding queries still leave a publishable Apply record.
After correction, 232129 actual-hook checks PASS, including recording OFF/ON,
each failed query, null bindings and 16000 successful transitions. Pixel shader
metadata/publication concurrency and replay surface/state-block receipt tests
PASS; the latter now verifies query failure stays sticky through EndScene.
Production and diagnostic x86 DLL builds PASS. The additional snapshot CPU
receipt executable builds but Windows refuses to start it (access denial/file
sharing violation), and a later check finds both generated executables missing;
the responsible external component is unproven. Its new query-failure assertion
is NOT RUN, not a pass. No protection setting was changed or file renamed.
No new Steam DLL deployed, no observed runtime query failure or flicker root
cause established, and no runtime FPS saving claimed.

Clarify existing GPUFrame log labels: D3D9 timestamps measure marker spans,
including submission gaps, not GPU Busy or full D3D11/OpenXR execution. The latest
10-12 ms values alone cannot establish GPU saturation. No profiler added.

Bounded next-cause audit: retained COM references do not preserve dynamic VB/IB
contents or mutable state-block Capture values. Existing captures snapshot each
first-person VB once, so they do not prove a same-frame overwrite in the menu.
Do not add per-draw readbacks or claim this causes the reported flicker.
Recreating an ALL/PIXEL/VERTEX block after Apply is not a proven equivalent clone:
Capture preserves the original state membership, including its light inventory;
a later newly created block can include additional lights. Custom EndStateBlock
masks are also unknown. No speculative replacement was implemented.
API contracts: https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dstateblock9-capture
and https://learn.microsoft.com/en-us/windows/win32/direct3d9/state-blocks-save-and-restore-state

P3 native input export (D46B image, EVIDENCE COMPLETE), private
d2vr-menu-registry-route-current.txt SHA256
69CA89D5E2214894A23D874E52763CDF6DE9024502B2D6F3D89D9D874CF0699E:
00560C40 assigns an intrusive reference to receiver+0C; it is not a registry
getter. 006EBBC0 validates a device through vtable+4, appends its reference to
manager+78, and returns a strong reference; it is not a mouse lookup. 00CE2B90
creates a reference wrapper. 008460E0 creates MouseDeviceWin, inserts it through
006EBBC0 and stores the returned reference at manager+A44. None is authorized as
a read-only lookup for menu injection. Live manager ownership, native menu mode
and the actually submitted interactive surface remain required before wiring
the existing pointer core. No native call or input hook added on this evidence.

## 2026-10-29: distinguish menu input ownership from platform factories

Read-only D46B Ghidra exports completed; private output remains outside Git.
Framework FC8930+124 dispatches 6C8080 -> C1FC50, which creates GraphicsSysImpl.
Its +128 dispatches 70D5F0 -> 40E8B0, which creates SoundSysImpl; neither is an
input getter. +12C/8B7800 returns the HWND at +F8. Base FC6800+128/E1391F is
__purecall. 9F4580 separately creates InputMgrWin through 63C950 or the
625670/8D89A0 paths, then passes services into client initialization C2C1A0.
Do not execute those factories to recover the existing manager.

C2C1A0 receives a client, not the outer framework. It stores distinct service
references at +2C4/+2D0/+374; their offsets alone do not establish input type.
The exact input
argument-to-field mapping still needs an independent calling-convention proof:
the decompiler's inferred stack arguments are inconsistent. A raw code search
also finds +374 in BFA900/AA07C0, but those methods' receiver classes are not
established. The private file named client-input-getter-confirmed does NOT
confirm that interpretation. No production field reader was added.

SWIG registration tables associate DA5FF0 with GameRulesImpl and DD2100 with
HumanPlayer; both IsPauseMenuShowing wrappers invoke the typed receiver's +80.
Their runtime SWIG type slots are 10F361C/11009EC, not live singleton addresses.
RegionMgr FC7D00+13C/490D30 returns a reference stored at +BC, but its concrete
receiver type and connection to HumanPlayer remain unproven. Do not call +80
on a guessed object. InputMgrWin+68/466710 is a timed update; +88/53EB30 invokes
acquisition helpers, and +B0/81D980 dispatches input processing. None proves
menu visibility. Native UI still has no renderer-owned finite menu plane.

Evidence SHA256: framework-input-create 728cddf55198d1235cb739b517d26a68311a324d1cab493d5526b82f2856c900;
framework-input-owned 5c694ed4843d9818ae41058d65a1a92887d8a100918d1b000bf6bba5c5514ba6;
menu-pause-receiver-owned 7d06ff735361d5b88972667699f0dfc2adeb302eb93387b133acde0c2d457d57.

The previously missing snapshot receipt executables were quarantined by
Bitdefender: quarantine metadata 40b309c8-cf3a-4d7e-a3a7-e686ed783d8b.dat and
e5460c49-ddde-4412-91d3-09c72e1c7492.dat identify the current and baseline test
paths and Gen:Variant.Draftor.34994. Their assertions remain NOT RUN. No restore,
security change or alternate executable retry was made. This supersedes the
earlier unknown-component note; it does not invalidate the separate completed
live-stateblock regression. No game launch, Steam change or FPS claim.

## 2026-10-29: integrate Opus inherited right-eye register correction

Fetched fix/right-eye-flicker-root-cause/5159cf4, based on 7543d92; reconciled
with MAIN 1e61a2e instead of replacing files. Keep the newer SyncActiveShader
failure receipt in replay and its fixture. Carry the right eye's successfully
materialized VS register rows past frame-start StateBlock restoration; preserve
Opus invalidation on unrecorded writes/Apply, replay Apply/failure, missing
replay, Reset and existing live-end restore failure. No accepted gameplay or
performance implementation was replaced. Full review, remaining state-block
scope and consolidated runtime gate: docs/research/replay-projection-history-20261028.md.

Baseline actual replay loop reproduces inheritedVariant=0/mode=0/draw=0 bank
mismatch. Final actual-source production/diagnostic loop passes 58370 checks
each (96 streams/576096 records), now including actual StateBlock Apply. Real
WVP/STP stereo math passes 64 changing pose/FOV scenarios without applying the
pose twice to carried constants. Projection history144416, live stateblock264146,
register bank783041, tentacle26629, weapon122025+3200 native body invocations,
hands2207, left aim461, throw316, right-eye color history and pixel metadata
fixtures PASS. Both x86 DLL builds PASS. Adaptive policy3243 PASS; host integration
NOT RUN (Windows sharing lock). Snapshot receipt NOT RUN (earlier quarantine).
Five other fixture failures reproduce identically before/after integration;
their missing fixture symbols/stale source assertions are documented, not waived
or reported green. No FPS improvement or HMD visual acceptance claimed.

Steam DLL remains af9cdd7 SHA256
7AB078B3089958F306040E3696FD4EBBD54A7EA0B64C3BC047AD186D6A98E2A4;
closed-wrist asset, profiles and SteamVR settings retained. No auto merge,
deployment or game launch. Next runtime check combines flicker, head motion,
periphery, shadows, hands and frame pacing with baseline rollback on regression.

## 2026-10-29: restore five stale actual-source regression fixtures

Follow-up to the Opus integration, with no production source or game changes.
UP geometry now compiles the actual RecordLiveResult receipt and verifies valid
geometry rejected by the driver, sticky failure, recording OFF and original
HRESULT/one native call. PS payload uses the current bound g_activePsHash
metadata rather than the obsolete pointer map (576 cases per build flavor).
Shadow draw extracts its actual four wrappers without unrelated world-draw
bookkeeping (covered by projection history). Projection estimator assertions
now require the current expanded upload range and both WVP/STP witnesses.
Sampler lifecycle checks admit exactly the three unconditional reset cleanup
calls preceding caps invalidation; actual lock/receipt bodies are compiled,
with sticky failed-frame and inactive-recording checks (4125 checks).

All five previously failing fixtures now PASS on x86, including both PS fixture
flavors. Earlier integration DLL builds remain applicable: production source
is unchanged. The two previously NOT RUN host tests remain NOT RUN; no security
workaround or executable retry. Runtime visual acceptance and FPS impact remain
pending the consolidated authorized headset session. Steam installation intact.

## 2026-10-29: Opus DirectRecordCopy manual Steam candidate

Integrated e491485 onto MAIN 9c831d5 without conflicts; Opus DirectCopy/filter
and recorder ordering retained byte-for-byte. Only the configuration admission
differs: default OFF, with Render/DirectRecordCopy read exclusively from the
diagnostic INI in DiagnosticCapture builds. The authorized Steam test profile
will use 1 (ON); ordinary builds remain OFF. Latest WVP/STP eye carry, gameplay,
hands, tentacles and performance code retained. Incomplete VR settings work is
saved in a separate stash and excluded from this candidate.

Both x86 DLL builds PASS. Actual recorder/memory fixture10681 checks; UP4403;
projection history144416; live stateblock264146; live status2848; replay surface
128 streams/896 calls; actual float replay58370 checks per production/diagnostic;
protected hands2207, left aim461, tentacle26629, head117021, XInput20041 and
gestures774 PASS. Source default/profile gate checked by the recorder fixture.
These do not establish PAGE_GUARD concurrency or protection-restoration safety.
The unresolved risks and manual-test-only scope are explicit in
docs/research/direct-record-copy-20261029.md; no runtime FPS claim.

Verified rollback preimage: working af9cdd7 DLL SHA256
7AB078B3089958F306040E3696FD4EBBD54A7EA0B64C3BC047AD186D6A98E2A4
and every active INI (two game and two local) under
%LOCALAPPDATA%/Darkness2VR/backups/direct-record-copy-20261029T162713.
Pinned EXE D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1
verified; elevated CIM sees no game, only SteamVR/fpsVR utilities. Deployment
receipt and immediate rollback are kept with the private candidate package.
No game launch, profiler, extra A/B session or SteamVR configuration change.

## 2026-10-29: resolve native client/input initializer ABI

Supersedes the earlier unresolved C2C1A0 argument mapping. Global service
constructor C5CC40 selects FBAC40; its +1C/74B5F0 getter returns this+50 with
plain RET, consuming none of the three preceding pushes. C2C1A0 receives seven
arguments and RET 1C. Actual push/frame/assignment instructions establish
framework+A8 -> client reference -> client+374 -> input reference. Sound and
graphics reference fields are +2C4 and +2D0 respectively. No live framework
address, menu state, receiver lifetime or native menu plane is established.
Details and next gate: docs/research/native-menu-input-contract-20261029.md.

New read-only audit_input_service.py verifies the installed D46B image and
decisive getter/initializer bytes, reports a static-only contract, and rejects
an altered image. Both checks PASS. No C++ source change or new x86 DLL build;
this evidence tool does not inject input. Private Ghidra exports all contain
EVIDENCE COMPLETE. Vtable/getter/framework-init export SHA256 respectively:
FCF8294610782EEC5F410AF443723A33226E96B3ACBEC30E09D31E4A703CDC7B,
0C15E4C53AFDB2DB2E3BB4AFD934BEA4C4E3E7077625D2622131C39B6A3A1481,
ABD4A924DD214FB7A3CCC8E09726F9BA5F12CFCFF775C68F8283059B8AE36312.

P0: retained outdoor C6 material-dispatch inclusive exposure is approximately
.252 nominal sampled ms/present, not a justified substantial optimization.
No redundant submission or new safe direct-copy ownership proved here. The
WOW64-only memset bucket remains unattributed to individual native callers;
record arena initialization was already removed. Preserve the current accepted
Steam DLL 28657E6A76964493FEDF6F6BB9C1C8E8976B0F94578147C1AB32F52537DE6784,
DirectRecordCopy OFF, and the separately deployed cb2cd4c Polish repair.

## 2026-10-29: existing HWND exposes native framework ownership

40DEC0 configures D993C0 and the framework creation parameter. C27160/D14750
copy them into graphics+1D0/+1D4, consumed by A0B540. D993C0's WM_CREATE
stores the framework in GWLP_USERDATA and HWND in framework+F8. The pointer
chain can therefore be observed without a new EXE hook or factory call.
Window userdata alone is insufficient: audit_input_service.py optionally
verifies the pinned loaded code, both reference indirections, framework and
Windows input-manager types, HWND mirror and matching repeated snapshots.
The query/VM_READ-only observation is not a lifetime lease or menu-state proof.
Missing snapshots exit with failure; no process launch or input writes.

Four ownership tests PASS (including wrong type/window, reference/object
confusion, invalid pointer, overflow and unreadable memory). Installed static
audit PASS; changed image and invalid PID rejected. Live snapshot NOT RUN:
game closed. The earlier minidump captures no framework heap instance and does
not close this gate. No C++/DLL/INI changes or runtime performance claim.
Native menu surface/state and safe input lifetime remain unresolved; details
in docs/research/native-menu-input-contract-20261029.md.

## 2026-10-29: identify RegionMgr's game-rules reference

Native GetGameRules registration 10A6150 -> D9B8C0 converts the receiver using
RegionMgr's SWIG type slot 10EBE00, then invokes +13C. FC7D00+13C -> 490D30
returns a retained reference from RegionMgr+BC (not the object), with EAX
output identity and RET 4. Wrapper assignment 59AB10 and native SWIG publication
confirm the returned reference route. Independent PE vtable/full-getter pin
PASS; all three private Ghidra exports complete. This narrows the native pause
state route without substituting render activity or a controller pause latch.
Live concrete receiver, main-menu ownership and submitted menu surface remain
unverified; no production virtual call/input injection or C++ change added.
Detailed evidence and hashes: docs/research/native-menu-input-contract-20261029.md.

## 2026-10-29: native camera coverage through the owned view boundary

Mode3 previously widened only camera+620 frustum data while the native scalar
FOV and projection remained narrow. Reuse the headset-tested camera coverage
setter/ownership/eye/HUD logic at the original near-getter boundary986139 in
the digest-pinned owned985D80 copy. The native EXE call/instructions are unchanged.
The native setter rebuilds projection/VP/frustum before their copies to the view;
existing binocular frustum refinement follows the setter.98618E remains the
pre-copy witness identity token; the mode3 callback now executes at986139.
No ZoneView flags or global visibility masks are changed. No new game pointers.

Static CFG proves986139 dominates FOV getter986161, frustum getter98618E,
projection getter9861B4 and VP getter9861C9 on paths reaching those consumers.
This establishes ordering, not the semantics of the unresolved virtual+9C.
Both x86 DLL builds PASS. Camera coverage7126, camera refresh48, actual-source
routing/coverage10000, owned-code ABI10000, owner span1000, instance1000,
binocular geometry249600 points and actual-source rebuild2000 calls PASS.
The dispatch fixture was missing current status/id/log stubs; repaired here.
MAIN still owns deployment and the consolidated Steam/PSVR2 check: peripheral
room/yellow edge, both eyes, head motion, HUD, shadows, LOD, hands and frame pacing.
Visual/FPS acceptance NOT RUN; right-eye flicker remains a separate defect.
Use Render/NativeViewBoundary=3 with the existing image-preserving profile;
Diagnostics/NativeCameraCover stays OFF. No deployment or game launch here.
Additional regressions: actual projection history145248, protected hands2207,
left aim461 and external-boundary/selected-camera fixture PASS. Boundary fixture
initially failed its corrupted-upload assertion once; diagnostic and unchanged
source reruns both passed. Cause not established; no acceptance check weakened.

## 2026-10-29: MAIN peripheral integration on a84fa35a

Integrated 8cde7ea8 without replacing the newer consecutive-L2 camera admission
or the owned-framework menu receiver correction. Retained both MODLOG histories.
Both x86 DLL builds PASS. Pinned installed-EXE CFG audit, camera cover7126,
refresh48, dispatch10000, external boundary, clone, geometry/rebuild,
projection history145248 and live stateblock296163 regressions PASS.
Hands2207, weapon125256, left aim461, throw316, Present observer,
XInput20041 and gestures774 PASS. These are offline checks, not headset proof.
User reports haptics and L2 culling PASS for the prior a84fa35a candidate.
NativeViewBoundary=3 and NativeCameraCover=0 already match the Steam profile.
Consolidated peripheral/HUD/shadow/LOD/FPS validation remains NOT RUN.
Right-eye flicker and native menu laser/cinematic-hand gating remain separate.

## 2026-10-29: retire tracked-hand IK during native cinematics

Native GetPlayingCinematic registration F82918/10A6130 selects D9B590.
Its RegionMgr+1AC dispatch resolves FC7EAC -> 6CF490: return &manager+268.
59AB10 copies that smart-reference handle; CA92C0 stores its replacement and
balances native references. Exact installed EXE SHA remains D46BB6C...8134EC1.
Private Ghidra exports cinematic-getter/owner/setter-20261029.txt in TEMP all
end EVIDENCE COMPLETE. No native code/decompilation exported to the repository.

Read only handle nullness within the validated native selected-view manager
context; retain no cinematic pointer and call no cinematic methods. Pin the
loaded getter/table separately so rejection cannot disable camera coverage.
Use only same-frame render-thread state. The private skin pack hook retires
our still-present IK edits, preserves either arm's newer native animation,
and dirties both packed skin slots only when bytes actually change. Native
packer ABI, reference consumption, LastError and FP state remain unchanged.
Actual-source hands2233, owner graph4178 and view callback176 checks PASS,
including cinematic entry/recovery, per-arm animation preservation, stale
frame, thread and contract refusals. Headset visibility acceptance NOT RUN.
Native menu laser still needs its native surface/input connection; existing
VR Settings laser is separate and is not presented as native-menu completion.
Both final x86 DLL builds and actual-source camera dispatch10000 PASS.


## 2026-10-30 � reject incompatible DLL build flavors before deployment

The b6d2bfbf failed session and 213bb4f7 recovery candidate used standard x86
DLLs. The installed profile requires the D2VR_MATCHED_CAPTURE protected-native
variant for ImagePreservingPresent/Input, native FOV boundary mode 3, protected
head/gestures/aim/throw/hands and native copy/preload optimizations. The retained
b6d log explicitly refused QueryPreload as requiring that architecture; source
preprocessor gates prove other required implementations were absent. The earlier
accepted 4A503FDD DLL contains ProtectedPlayerOwner, whereas the standard variant
does not. This packaging mistake is concrete; it does NOT isolate the reported
17 FPS / 70 ms or AppHangB1 to a single mechanism.

Both variants now log a retained build-flavor marker. The read-only deployment
gate checks x86 PE DLL identity and required profile features before any target
mutation, without loading the DLL. Build-install, research, haptics and legacy
candidate installation entrypoints use this gate. The protected-native build
keeps matched capture disabled by the existing INI. ReplayStateFilter and
ReplayBufferVersions remain OFF; the failed Polish pack is not reinstalled.

44 real-DLL profile tests PASS, including standard/protected feature matrix,
local override, absent profile, bad PE/architecture and an actual rejected
research-install preserving the target bytes. Both x86 DLL builds PASS. Native
owner graph 4178/view callback 176 and VR Settings input4518/renderer863/host50/
submission7215 PASS. Existing shadow warnings in the owner fixture remain.
Steam candidate correction restores the supported build variant and preserves
accepted profile values; physical visuals/FPS/hang recovery remain NOT RUN.
No native menu laser completion, half-rate forcing or new FPS gain is claimed.
UEVR OverlayComponent was reviewed as a design reference (actual surface and
input ownership); no source copied and no new license obligation introduced.


## MAIN 2026-10-30 - replay binding and metadata, staged offline

Integrated SIDE 38fe1fee/e6268ae6/f2a3c4f3 onto accepted protected-native
1dd6205e in codex/main-render-cpu-next. All recovery switches remain OFF.
Both x86 builds, 44 binary/profile checks and affected actual-source replay,
buffer and stereo regressions PASS. Snapshot CPU execution remains BLOCKED
by Access denied; implementation and fixture match the accepted base.
Bounded metadata reuse is bypassed with versioning OFF. No Steam or localization
changes while the user tests fonts. Runtime FPS and visuals remain NOT RUN.
Details: docs/research/replay-binding-filter-20261030.md.


## MAIN 2026-10-30 - observed native menu receiver correction

The accepted candidate's retained refusal receipt resolves the input object
as native FBFAD8, not the previously assumed derived FBFCD0. Pinned Ghidra
constructor/initializer evidence proves the same +A44 MouseDeviceWin reference
layout. Added only this exact known type, including type identity in snapshot
equality; no native calls or injection. Actual-source regression FAIL before /
PASS after, 149 C++ checks and five Python ownership tests PASS. Pinned private
EXE audit and both full x86 builds PASS. The initial Python test edit contained
a C++ integer suffix; corrected before the passing run. Native menu laser
connection remains incomplete. Steam and accepted 5ef2106c baseline unchanged.

## Native menu mouse ownership transaction (2026-10-30)

Added `src/native_menu_mouse_merge.h` for the independently recovered D46
20-byte DirectInput polling boundary. It queues every logical press/release,
merges ownership with fresh physical button state, preserves unrelated events,
defers edges when the native buffer is full, and leaves failed/peek/unknown
calls untouched. Overflow defers queued clicks because 9A5D90 can rebuild events
from immediate state. Cancellation discards undelivered clicks and retains a
release for an already committed synthetic press. No allocation, Windows call,
lock, native call or retained native pointer is introduced.

Actual-source x86 regression PASS: 172147 checks, including 4096 six-poll
physical/laser transition sequences, physical-button ownership, quick clicks,
full/overflow buffers, malformed calls and queue exhaustion. This is the merge
component, not a connected native menu laser. The host must use fresh owner-
thread buffers after native polling, handle queue rejection by cancellation,
and supply actual submitted menu geometry. Steam baseline 5ef2106c unchanged.

Closed the alternative WndProc route: Client base constructor 5CBF10 installs
FDDAD0; slot +B4 selects 97E160, which queries its +2A4 referenced object's +68.
Client initializer CA49E0 creates that reference from DarkitectClientImpl.
It is an engine-tool receiver, not a demonstrated native game-menu predicate.
Do not route laser clicks through its +17C Windows-message callback.
Complete private exports remain in TEMP/d2vr-menu-client-*-20261030.txt.


## Fresh native mouse poll ABI (2026-10-30)

Added an x86 GetDeviceData gateway capturing the current ESI owner and exact
return address before compiler register reuse, plus strict value-only native
mouse context admission. Pinned the complete constructor and inline 20-byte
state layout in the existing private-image auditor. No native hook, injection,
EXE write, menu-visibility assumption or Steam deployment is introduced.
Actual-source x86 gateway regression PASS (18071 checks / 3000 ABI calls),
existing mouse merge PASS (172147 checks), pinned EXE audit PASS. Full native
menu laser remains incomplete pending activation/surface and host integration.

## Optional local constant policy

The owned-constant classifier accepts an optional compile-time private policy.
Without that file it preserves the existing public site set. Source address,
extent, owner/frame/parent checks and image admission remain unchanged.
The private policy and its private image auditor are explicitly gitignored;
no new executable addresses, offsets, fingerprints or image bytes are published.
PublicPolicy on the existing x86 fixture verifies the baseline independently.

Validation: full local eighteen-site extension 413273 actual-source checks PASS;
public-only policy 412337 checks PASS; both x86 DLL flavors PASS. Previous
buffer/Reset/projection regression results remain applicable: those source paths
are unchanged. No measured runtime gain, deployment, or MAIN modification.
MAIN needs the locally retained policy header to build the eighteen-site extension;
a public checkout alone deliberately does not activate unpublished contracts.


## 2026-10-30 — Carry owned-copy coverage onto tested mapped-copy baseline

Rebased the optional owned-source policy onto 21e9b5b5, preserving MAIN's deployed
mapped-copy implementation. The deployed baseline still has only the previous
field table; the previously validated eighteen additional submission sites are
included in the local ignored policy for this candidate, not in public history.
Build with that local header to exercise the full candidate. Public checkout alone
retains the original coverage. DirectRecordCopy and ReplayBufferVersions remain
unchanged; no profile or Steam changes.

Validation on this combination: 413273 private-policy and 412337 public-policy
actual-source x86 checks PASS; private producer-contract audit PASS; both x86
builds PASS; six VB/IB cases, NOOVERWRITE, Reset/lifetimes, recorder reentrancy,
UI ring DISCARD and worker callbacks PASS. Offline gate/copy/stub-forward for
64 bytes: RPM 707.8 ns versus owned 53.9 ns. No per-frame saving is claimed:
runtime frequency of these extra sites is not measured. This is integration of
the retained optimization, not a second mapped-copy fix or a new discovery.
