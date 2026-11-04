# Fenced input rotation and native material shimmer, 2026-11-04

Status: experimental, default OFF. User resolution2676x2728 preserved. This is
partial evidence, not recovered outdoor40FPS or a complete headset shimmer fix.
Parent source f52d21ab, MAIN branch codex/combined-scenes-input-shadow-20261103.

## Shared inputs: preserve ownership before overlapping more frames

`SharedCaptureRotation=1` alternates two pairs of input surfaces only with
split capture, transfer mode4, deferred readers and last-consumer fences.
Outputs still belong to the current stereo pair. All four slots retain their
own producer/reader receipts and are drained before Reset/free. Failed extra
allocation falls back to the original pair until a successful Reset. Extra
input memory at2676x2728 BGRA8 is about55.7MiB; no eye-resolution change.

Actual x86 D3D9Ex/D3D11 transfer regression includes180 alternating four-slot
transfers,20 at full dimensions, distinct frame/eye colours, isolated right-eye
and UI background checks, retirement before bank reuse and drain before Reset.
The test's total is3040 transfers/1100 at full size. Reader-fence protocol2000
jobs and isolated-eye pixel/history/reset fixture101169 checks also PASS.
Rotation with adopted colour-history reuse is not claimed tested by this case.

Private lab `test-output/shimmer-repeatable-20261104`:

- A PID38348, rotationOFF, restaurant save: reader mean3.139..3.238ms,
  left6.37..6.75ms, xrEndFrame about0.22ms; boundary10.31..11.86ms.
- B PID52116, rotationON: reader mean0.007..0.011ms, left0.06..0.10ms,
  xrEndFrame4.969..5.491ms; boundary7.955..8.335ms. Roughly60 submissions/s
  in both cases. Waiting moved; no demonstrated FPS gain. Some later reader
  spikes remain. Scopes overlap and must not be added.
- A2 PID45348 and A3 PID22280 only reached menus: INVALID reverse gameplay
  controls. C PID49628 reached the actual restaurant/world with rotationOFF,
  and reader means later fell below1ms without rotation. Scene phase matters;
  A/B above is insufficient to adopt rotation as an optimization.
- Runtime preview counters/target90Hz are not measured physical refresh or
  proof of reproducing the user's outdoor40FPS event. Captures and thread
  snapshots perturb timings; measure separate clean windows.

## Native skin material instead of another full-image blur

Explicit draw sampler capture found full mip chains on hand materials; native
diffuse/normal samplers already use aniso16. Normal mip selection is POINT,
not trilinear. This does not justify another global aniso override. Native
material BC938BC9 reconstructs unit Z from XY, losing mip normal-length
variance. A Toksvig length estimate cannot recover that discarded information.

`SpecularFootprint=1` changes exactly the scalar Phong power in that digest-
pinned425-word PS (SHA256 da8c8cec1a80e033748b03bf5ac5b7d68673f686af0e89b34318f288be730a27).
Four normal samples at UV +/-ddx/4 +/-ddy/4 produce four reflected-light powers;
their mean replaces that power. Native centre samples still drive diffuse,
ramps, shadows and sheen. Four extra normal reads, unused r24..31/c223. Exact
digest/size/instruction gates; creation failure falls back to native shader.
World draws sharing this exact material are affected too. This is not mesh-edge
AA, a temporal filter or a change to NIS strength. Default OFF.

`test/build_specular_footprint.ps1 -NativeShader <private ps-BC938BC9.bin>`:

- PASS native D3D9Ex shader creation on RTX5070; all425 single-word mutations,
  null/truncation rejected. Private native bytecode is not committed.
- Flat normals/gloss1,16,128,512: red-channel max byte error0. Diffuse checker
  image with specular disabled: identical red-channel pixels. These checks do
  not constitute an all-material/all-channel gameplay proof.
-32 moving phases for each native/modified material: mean0.443628 ->0.404018,
  standard deviation0.276016 ->0.083023, normalized variation(CV)
  0.622177 ->0.205492, about67% lower. Synthetic motion of the actual native
  PS, not a measured improvement in headset hands. Quality gate requires20%
  CV reduction and retained energy; the prior derivative-variance filter failed.
- Full-screen material drawn twice at2676x2728, alternating order,4 warmup and
  28 retained measurements, timestamp disjoint checked: native median0.2037ms,
  candidate0.3129ms (p95 0.2044/0.3143). Stress fixture, not gameplay FPS.
- Diagnostic and production builds PASS. Actual game PID49628 logged successful
  patched BC938BC9 creation(result00000000) and showed both hands in emulator.
  Actual moving-hand quality/clarity and HMD acceptance remain NOT RUN.

## Native outlines: actual source identified, no guessed patch

E345B150 paired with VS AE773A48 is active. Explicit capture PID49628 serial1201
frame24479 draw449 saved colour RT before/after its draw with separate hashes.
Its input depth(INTZ) and normal/edge mask(A8R8G8B8) are both2676x2728, one mip,
POINT min/mag. VS c0.xy is1/2676,1/2728; four neighbour offsets are one native
pixel. The PS reads five depth and five normal samples, thresholds depth/normal
differences and writes edge colour/opacity. This is not evidence for a small
hidden render target. A half-resolution depth/particle pass elsewhere is not
evidence that all game colour or hand rendering is low resolution.

Theater captures have two quad layers and incomplete projection completion
receipts. Raw draw/material/stage files are useful, but must not be labelled
complete matched projection captures. No weakening of capture validation.

## Native sleep attribution

`Diagnostics/NativeSleepTiming=1` is an explicit diagnostic-build-only external
SleepEx observer. Native image guard and original Present readiness are required.
Existing verified engine wrapper00998130, API import00EEB358 and native return
0099813D gate admission. Parent return00938B67 identifies the actual deadline
loop, not every engine sleep. Record arguments, elapsed wall time and caller
counts on the render thread only, aggregated every5s. No sleep removal, affinity,
timer-resolution setting, EXE/IAT patch or production collector.

Actual x86 MinHook/Windows test reproduces wrapper/parent ABI in dedicated owned
test-image padding. PASS normal return, LastError preservation, alertable APC
delivery/WAIT_IO_COMPLETION and unrelated-caller exclusion. A first fixed-address
VirtualAlloc fixture safely refused a pre-existing reservation; it was replaced
by explicit test-image padding, never by overwriting that reservation.

Game PID42132 observer installed and saw actual deadline calls. Initial intro
windows had0..4 calls/5s, about0..7.178ms total wall time: the deadline SleepEx
path is not an explanation for all current intro frame time. Gameplay/low-FPS
correlation is still required. Raw stack words from wall-clock snapshots are
not sampled CPU execution or a verified unwind; don't use them as ETW evidence.

## Evidence and remaining gates

Private build logs: capture-rotation-{diagnostic,production,reader-test,eye-test,
interop-test}.log; specular-supersample-gpu-test.log; specular-edge-diagnostic.log;
specular-production.log; native-sleep-{test,diagnostic,production}.log.
Lab raw logs and matched captures remain private, outside commits.

Open: real moving hands before/after, geometry/outline shimmer, clean equivalent
world performance controls, user's outdoor25ms CPU/40FPS repro, and HMD acceptance.
Do not merge/push, claim complete, enable experimental defaults or shut down on
the strength of these offline/emulator results alone.


## Scheduled CPU and reader polling backoff (checkpoint)

Native swapchain GetPresentParameters reports windowed=1, interval=0x80000000
(D3DPRESENT_INTERVAL_IMMEDIATE), DISCARD, one2676x2728 backbuffer. Observed only;
no VSync patch. Native cap readback is120, FrameCapMode0. Native deadline SleepEx
has zero calls in measured world windows, so it does not explain those windows.

CPUExecution now uses GetThreadTimes kernel+user deltas, only with explicit
CPUSyncTiming diagnostics. Same-thread and monotonic guards; ordinary disabled
scopes make no thread-counter calls. Wall spans and scheduled CPU remain separate.
Counters are quantized and scopes overlap, so do not sum their reported times.
Real Windows x86 fixture PASS:191ms sleep approximately0 CPU;180ms busy loop
171.88ms CPU; cross-thread stamp and disabled-scope exclusion PASS.

Opt-in SharedReaderSleepUs (default0, maximum1000) uses a private thread-local
high-resolution waitable timer after four yield polls. Diagnostic override:
SharedReaderSleepUsExperiment. It changes no global timer/scheduler settings.
The original D3D11 query must STILL return S_OK; timeout, ownership, retirement,
reset and same-frame ordering are unchanged. Unsupported timer falls back to yield.

Actual D3D9Ex/D3D11 fixture PASS3040 transfers,1100 full2676x2728, distinct eyes,
four-slot retirement and adopted histories using200us backoff. Diagnostic and
production builds PASS. Private logs gpu-query-pause-{interop-test,diagnostic,
production}.log. No physical headset acceptance.

World F (backoff0): example301-frame/5s window reader828.125CPUms/602 waits,
about2.75ms per frame. World G (200us): example300-frame/5.015s window
156.25CPUms/600 waits, about0.52ms per frame. Frame cadence remained about60.
Whole render-thread CPU did not show a corresponding reliable decrease across
these different scene phases. These are preliminary reader-scope observations,
NOT a controlled FPS improvement. F also had a148-frame/5s window requiring
further attribution. H reverse control is EXCLUDED: captures show title/intro,
not the world scene. Do not compare its timings with F/G world windows.

The original user outdoor40FPS/25ms CPU case is not yet reproduced. Keep this
experiment OFF by default until equivalent world controls and runtime acceptance.


Correction to F148-frame window: exact log correlation shows native Present
returned08760878 (S_PRESENT_OCCLUDED),662 unadmitted presents, then08760877
(S_PRESENT_MODE_CHANGED). It is an occlusion/display-transition window, NOT a
clean gameplay performance sample. Exclude it from controlled FPS comparisons;
do not present it as reproduction of the user outdoor CPU25ms case.


## Scoped first-person normal-map mip interpolation

Captured skin VS02A9CDE3/PSBC938BC9 uses1024x1024,11-mip NormalMap s2 with
ANISOTROPIC min16 / LINEAR mag but POINT mip. SkinMipFilter (default OFF) admits
only this pair, first-person viewport MinZ0 and0<MaxZ<=.05, exact1024/11-mip
non-RT/non-depth texture. Change only s2 MIPFILTER POINT->LINEAR around the draw;
restore original state in both live/replay paths and invalidate material cache
through its existing bypass. No recorded sampler commands, global aniso or bias.

Actual Windows x86 D3D9 GPU test PASS: an LOD1.49->1.51 transition changes255->0
with native nearest mip,130->126 with scoped interpolation. Destructor restores
POINT; rejected world viewport, null/single-mip texture, already-linear state,
failed setter and unknown shader identity are tested. This proves interpolation
and scope restoration, not headset shimmer removal. Both builds PASS. Game I/J
logged application eye0 AND eye1, native shader creation success and visible
hands/weapons. Actual moving-hand temporal comparison remains an open gate.

## Live reader A/B/A, same process J

Explicit diagnostic LiveRenderExperiments enables render-boundary polling for
reader backoff and scoped skin mip only. OFF by default, absent in production.
No persistent query/resource identity is changed by either control. Private
phase receipts and whole logs are j-ab-phases.json, j-{A200,B0,A200repeat}.log.
Exclude startup and straddling counters; use a complete5s window before the last
window in each16s phase. Final partial windows are excluded.

Reader CPU per frame:0.573ms at200us,2.396ms at0,1.042ms at200us again.
Whole scheduled CPU per frame:10.052,9.167,9.479ms respectively; cadence60.0,
60.0,59.81. Thus reader-local saving is repeatable, whole-frame CPU/FPS gain IS
NOT established. Scene advances/restarts when the idle player dies; no claim of
identical game simulation across phases. Keep reader experiment OFF for users.
Game J actual projected combat capture j-shoot-test.png has both guns/hands;
other captures include expected scripted/theater segments and are not equivalent
free-world controls. Original outdoor40FPS is still not reproduced.

## Advisory GPU completion events and Villa reproduction

Added a diagnostic-only startup switch SharedReaderCompletionEventExperiment.
ID3D11Device5 fence + ID3D11DeviceContext4 Signal + SetEventOnCompletion replace
busy polling only as an advisory pause. The ORIGINAL exact event query returning
S_OK remains the sole resource-reuse authority. Context lease/thread checks,
worker receipts and Reset rejection stay intact. Unsupported APIs/failures use
the old pause. Pending registrations persist across diagnostic live A/B and
are drained before freeing slots. A failed drain retains slots and refuses Reset;
the helper also retains its registration on exceptional destruction timeout.
Default OFF, unavailable through production configuration.

Official contracts checked:
https://learn.microsoft.com/en-us/windows/win32/api/d3d11_4/nf-d3d11_4-id3d11device5-createfence
https://learn.microsoft.com/en-us/windows/win32/api/d3d11_3/nf-d3d11_3-id3d11devicecontext4-signal
https://learn.microsoft.com/en-us/windows/win32/api/d3d11_3/nf-d3d11_3-id3d11fence-seteventoncompletion

Actual x86 GPU A/B/A/B, 400 iterations/phase, 32 full-resolution copies/iteration:
yield CPU703.125/703.125ms versus event15.625/below-one-counter-quantum ms;
mean wait1.7858/1.7791 versus1.8269/1.8238ms. Below-one-quantum is NOT zero cost.
Source test/build_gpu_completion_pause.ps1; private log
build/gpu-completion-pause-results.log. Subsequent concurrent-game fixture run
is functional evidence only; GPU contention invalidates its clean timing comparison.
Actual D3D9Ex/D3D11 interop PASS3040 transfers/1100 full-size with event pauses,
both-eye pixel identities/history/reset and original-query retirement retained.

Fresh lab test-output/gpu-completion-lab-20261104 backs up current user4A/3A
directly; it does NOT use the older E56 rollback. Simulator override is process
local and source2676x2728 unchanged. Restaurant startup test reached world and
skin mip applied to both eyes, but its death-loop timings remain uncontrolled.

Original user's Villa save yields a stable elevator/lobby view, both hands
visible and following four distinct acknowledged controller poses. This is
movement/visibility evidence with skin mip + specular footprint, NOT headset
shimmer acceptance. Private hand-pose-{0..3}.png and hand-pose-acks.json.

Villa same-process event/yield/event/yield comparison (complete5s windows,
startup/straddling/last partial excluded): cadence40.67/41.8/43.8/43.6;
scheduled render CPU21.06/19.51/19.48/20.00ms-frame; reader CPU below counter
resolution in ALL four phases. Thus NO FPS win from completion-event waits.
This newly reproduces a CPU-heavy roughly40FPS case, but is an indoor Villa
case, not proof of the original enemy-heavy outdoor cause. Final images confirm
the same idle lobby scene, no death-loop transitions. Private ab-summary.json.

Eight-second wall-context sample474 observations: native EXE171, system D3D9
42, proxy85, ntdll109; these are NOT scheduled execution percentages. Two
same-named D3D9 modules must be matched by full path/base/size, never name case.
Native proxy was559C0000 size12140544, systemD3D9 was664D0000 size1544192.
Sampler now preserves full module paths and unwraps Windows PowerShell's
serialized array value wrapper. Interval1ms is requested, NOT guaranteed;
actual observed cadence is about59 context observations/sec. Map proximity
is a lead, not a complete symbol/unwind attribution. Further bounded sampled
hook timing is diagnostic-only and labels nested wall scopes non-additive.

Builds for event code PASS production and diagnostic; actual-source binding
regression PASS1679 checks. Old build-small-record-copy.ps1 currently fails its
startup-order source check because submission QI/software hooks precede its
copy-admission marker. No fixture PASS is claimed; its recorder C++ fixture
now includes the new diagnostic timing declarations. This source-check issue
predates the event/timing changes and remains separately open.

### Expanded hook costs, scheduling context, and coverage sampling
Latest diagnostic adds bounded sampled wall scopes for total/driver draws and
texture/render/sampler/shader bindings. Nested scopes must NOT be added. Villa D
late window283frames: native VS1.42ms/frame, PS0.79, record payload0.684,
record command0.588, prepared lookup0.193, total draw0.983 (driver0.875 nested),
texture binding0.274, render state0.098, sampler0.089, VS/PS binding0.208/0.205.
This does not support promising a large win from another texture-binding cache.
Actual-source binding regression PASS1679. Production/diagnostic builds PASS.

D early game scheduled CPU22.6ms/frame versus late11.98ms/frame, without a DLL
change. Concurrent Python PID10228 consumed17,765.625ms CPU in about5s, while
several SteamVR processes also ran. Python command prefix was tools/gen..., but
full command was not retained before it exited. Do not infer which script it
was or claim causation: NPC state also changed. No other process was stopped.
Private d-process-cpu*.json and d-profile-complete.log preserve observations.

Real NVIDIA RTX5070 D3D9Ex hardware fixture samples moving triangle coverage at
32 phases. RMS error versus area reference: native0.167464, SSAA1.25 dimensions
0.102270, SSAA1.5 0.085743, SSAA2 0.048615, MSAA4 0.133810. Flat interior pixels
remain exact. SSAA1.25 reduces this synthetic RMS by38.9%; NOT a headset result.
MSAA4 fails the stricter50-percent error-energy gate and is not selected.
INTZ rejects hardware multisampling2/4/8 on this adapter (8876086A), although
color formats and D24S8 support it. Native sampled-depth/MRT ownership therefore
prevents treating MSAA as a simple output-target flag.
Sources: test/coverage_sampling_gpu_test.cpp; private build/coverage-sampling-gpu.log.
Microsoft contracts: https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-stretchrect
and https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-createrendertarget

SourceQuality5 is a separate explicit DIAGNOSTIC experiment, bounded4096/16Mpix;
existing0..4 retain their old8.2944M cap. Old test wrongly expected Exact3200x3264
to pass despite that cap; corrected without enlarging existing modes. Source
observer ABI/caller/window/failure/original-image tests PASS five variants.
New lab requests3345x3410 (1.25x each dimension) while XR remains2676x2728.
Installed diagnostic A119778BAA847A46559F159AC7E98D554FE5B0CABD4DD8640ACEAB2DF9E7F94F.
Startup RT0 matches requested size; upstream geometry/depth and gameplay cost
are NOT YET VERIFIED. Initial Quest-default run is excluded; lab settings now
explicit PSVR2. First PSVR2 run only reached intro video, then native scene
counter advanced without swapchain presents. It is not gameplay evidence.

Fresh simulator controllers start untracked: pointer actions stay inactive and
expire. Initializing both controller poses yields actual input_delivered acks.
tools/simulator_control.py initialize now performs acknowledged initialization.
Do not change the simulator's action-validity checks to hide this condition.

Community comparison: UEVR issue299 reports low CPU/GPU utilization with poor
performance, without establishing our engine's cause:
https://github.com/praydog/UEVR/issues/299
Valve discusses specular aliasing as a separate VR rendering problem:
https://media.steampowered.com/apps/valve/2015/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
EDVR's own AA investigation also distinguishes texture/specular aliasing from
geometry coverage; these are comparison leads, not transferable fixes:
https://github.com/characterecho-sean/edvr-unofficial-patch/blob/main/docs/anti-aliasing.md

The supersample-lab wrapper successfully restored every original payload and all4
save/settings hashes; private post-lab-restored.json. Second run also intro-only,
no world performance claim. Escalated READ-ONLY GetWindowRect confirmed native
HWND11207886 at(-712,-1165)-(1228,-65): entire window above monitor. Coordinates
match centering a3345x3410 request on1920x1080 while retaining the physical1940x1100
window; this is a layout-side-effect hypothesis, not disassembly proof.

Follow-up narrows Mode5 to verified resourceCaller only. General native size
call still discovers/validates HWND ownership but returns original physical
RECT, including native last-error and one forward. Existing modes unchanged.
Fixture explicitly checks physical general RECT and3345x3410 resource RECT,
including deferred discovery. Five variants PASS. Diagnostic build PASS.
Fresh resource-only lab candidate SHA0846EC2095B737D98B11B47EBDBEDEABE9C45A7C279A2F4D06BD8E046E556BA9.
Runtime investigation continues in test-output/coverage-resource-lab-20261104;
use supersample wrapper Restore for that lab when closed.

Resource-only runtime PASSED actual Villa loading, both hands visible. One
matched capture COMPLETE at TEMP/Darkness2VR-matched-10552-104-11217, draws888/eye.
1738 draw records use3345x3410 viewports; edge sampler INTZ depth is3345x3410;
post inputs A16B16G16R16F and INTZ3345x3410. Both final eyes2676x2728. This is
actual higher-resolution native geometry/depth, not enlarged output alone.
Private world-villa.png/world-final.log preserve scene and measurements.

Current cadence39-40FPS and scheduled render CPU~22.6ms/frame are CONTAMINATED:
5.10s system sample: Python46072 CPU19078ms and39288 CPU16406ms (~6.96cores
combined). Read-only CIM identified both as tools/generate-polish-natural.py,
separate asylum dialogue batches with --threads2; SteamVR additionally busy.
No process was stopped. Do not attribute this result solely to supersampling,
the game, or a suspected regressed mod. Original user's low-FPS cause remains
unproven. Full command paths remain private in world-background-context.json.

Resource-only lab ENDED: exact user DLL/INI and all4 saves restored by wrapper.
One-shot capture INI reset to its receipt-verified candidate before rollback.
Mode5 startup now also rejects missing/non-distinct allocation caller (fail
closed); this adds guard tests, not a runtime-size behavior change.
HMD edge/hand shimmer acceptance and uncontaminated whole-game FPS remain OPEN.
