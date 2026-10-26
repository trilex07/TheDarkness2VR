# Protected controls/head/gestures — one authorized Steam test

Candidate: `4281f29`, `codex/protected-vr-integration`.
DLL SHA-256: `57cf819e45abacf18a886d9fc42c4acdccc4c5b931d610944ec706cf9a6e91a2`.

## Observed game behavior

The user tested in PSVR2 and reported movement and gestures working. The followup
on head motion/stereo/peripheral visibility was "it was okay, only blurry" plus
20–30ms CPU outdoors. This accepts the reported controls/geometric view behavior
in this scene; it does not prove full native-camera visibility/collision, every
button, recenter, or every gesture. Native human hand/weapon pose and controller
ray pickup/throw remain disabled and unresolved.

Steam launched PID19352 at15:03:28 local and recorded exit0 at15:11:52.
There was no logged C0000005 recurrence. Three first-chance E06D7363 events were
recorded, including UIAutomationCore on a non-render thread during shutdown;
these are not an unhandled-crash finding or proof of long-term stability.

Protected native additional-swapchain Present reached sampled counts
entries=forwarded=returned=ok=37000, HRESULT0, recursion/errors/statuses/fault0.
Detach recorded37492 total Present observations; the final492 returns are not
individually logged. The mod's main-image write guard remained enabled; native
unsafe hooks, source sizing and reference-retention optimization stayed OFF.

Actual engine XInput polling and an active XR packet forward were recorded.
Both runtime controller paths reported oculus/touch_controller through SteamVR;
this is the runtime profile, not a claim that the hardware is an Oculus device.
Seven protected right strokes and two reload detections were logged. No objective
detection was recorded. User feedback is not split into acceptance of every action.

Geometric head application was logged66 times. There are25 sampled gameplay
FrameContexts with layers1, each pairID matching its frameID. Their first-to-last
96.322s span includes gaps and headset removal: it is not continuous projection
submission, simulation FPS, or a matched image capture. No extra capture or ETW
session was started. Native selected-camera/culling ownership remains unresolved.

## Clarity/performance are not fixed

Actual source remained1920x1071; larger XR eyes do not prove source detail.
The user still sees blur and20–30ms CPU when going outside. Target90FPS is NOT MET.

Existing logs contain120-sample GPU/CPU summaries, not per-frame execution and
wait attribution. Active slow cadence near the outdoor report includes53.8
layers/s and coarse p9532ms, adjacent D3D9 live+replay GPU means15.349/13.106/
13.621ms. These exceed11.11ms but cannot be assigned to a named CPU wait. The
nearby StereoPerf sums2.52/2.11/2.29ms cover only Present-side eye capture,
history/replay and restoration: native main simulation, live recording, some
frame pacing and original Present are outside those scopes. GPU utilisation or
the D3D11 compositor overlay alone cannot replace the D3D9 timing evidence.

CpuSyncTiming was OFF. Review also found that the protected Present callbacks
omit the existing periodic stage report and boundary/actual-forward measurement
that the legacy path had. A minimal reuse of those existing timings is being
implemented before the narrow reference-retention A/B; this is an instrumentation
correction, not an FPS improvement. No new performance framework or general ETW
survey is requested. Repeated COM references remain a specific reversible
hypothesis; recorded requests reached2541/frame with mode0, but their runtime
cost and duplicate count remain unmeasured.

## Preservation and next step

Original EXE SHA/PE/instruction audit passed before launch. Fresh backup covered
DLL plus all four INIs. The five originals were restored after process exit and
all hashes matched. EXE SHA remained
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
The known-good rendering package and newly tested combined candidate are retained.
Private raw logs, package manifests, byte audit and rollback receipt stay outside
Git. Only this bounded receipt is committed.

Next candidate preserves the accepted controls/head/gestures and source size;
A/B changes only UniqueReplayResourceRefs0/1 with identical existing CPU-stage
measurement enabled in both. The missing protected report/forward timing must
first be corrected offline. New Steam launches still require user authorization.
No GPU/CPU improvement or90FPS is claimed before a comparable test.
