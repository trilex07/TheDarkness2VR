# Verified engine presentation boundary — 2026-10-21

## Runtime evidence

The tested x86 executable has PE timestamp `4F68A875`, image size `00DEC000`.
At RVA `694052` the engine executes `mov eax,[ecx+0Ch]; call eax`, after
pushing six SwapChain::Present stdcall arguments. Return address is `00A94057`.
The new five-byte CALL preserves these arguments and the original HRESULT, and
resolves slot 3 from the actual argument's live vtable on every call. It does not
modify a D3D implementation entry or a swapchain vtable. Other executable builds
are rejected by PE identity and instruction-byte checks.

The old hook watched the wrong COM object. In PID 40116, the intercepted actual
swapchain was `0DBF0E20`; its GetDevice returned `0DAB9FA0`, whose GetSwapChain(0)
returned `0DAE9600`. These are in-process COM observations, not an inference from
ETW addresses. GetNumberOfSwapChains returned 1. The alternative engine branch at
`00A94072` is not intercepted yet; the observed session used `00A94052`.

The old implementation also allowed a recursion cycle. WER dump PID 28440 has
16,319 repetitions of the same 256-byte stack pattern, including our Present
reentry branch and overlay return addresses. Steam's reported fault is in its
stack-probing/reporting path; module blame alone is not a root-cause diagnosis.
Live inspection showed inline JMP patches on both the system Present entry and
our replacement function. Calling a saved original that reenters our function
was not prevented by the boolean guard: the guard called that original again.
Moving the vtable installation from Reset to EndScene did not solve this.
The call-site implementation leaves those entries alone and avoids this cycle.

## Real tests (OpenXR and replay disabled)

- DLL `3D9F6FEDA0A1161BB45FA25F0CE9FC64202A38D7A37EADEAEA98939CBA1D0CAF`:
  PID 14808, 23:16:01–23:20:02. Over 63,000 completed calls, menu and loaded
  restaurant/barber level verified by window captures; closed through WM_CLOSE.
- DLL `CAA9D84A188CFE5757B8D58483F0B72CDBA287C26D568CC645BF72BC375AD9D7`:
  PID 40116, full BeginScene/EndScene/Present trace. Captured 67,472 Present rows
  over 134.532 seconds, including 51,914 consecutive D3D_OK results, all on thread
  38760. No negative HRESULT. Later results include S_PRESENT_OCCLUDED (548) and
  S_PRESENT_MODE_CHANGED (15,010); they must not be counted as displayed frames.
  Menu/title, loading/cutscene and loaded level were visually observed. Process
  detach counter was 67,502; the live CSV copy is a prefix, not a complete capture.
- Steam overlay module was loaded in these successful tests.
- Frame-boundary suite: 2,327,478 assertions, zero failures (50,000 generated
  traces and 25,000 timing simulations). This is unit evidence, not VR evidence.
- Independent PresentMon attempt: INVALID, 323,227 lost ETW events; no usable
  output obtained. Do not claim agreement with ETW.
- Overlay ON/OFF: repeated with Steam's actual Settings > In Game toggle.
  ON PID 44636 exceeded 63,000 calls; OFF PID 31084 exceeded 23,000 calls.
  Both used DLL `3F514A09569DF01AA4AA98EC2D65EB48B845FC1D05F7639F1CC510D558E342D3`.
  The overlay module and inline hook remained present even with the UI toggle
  OFF, so this is a setting comparison, not isolation of all Steam code.
  The original UI setting (ON) and original registry value were restored.
- A second minimal PresentMon attempt also failed (7,491 lost events, no CSV).
  Independent ETW agreement remains unverified.

Raw local evidence is under `test-output/`: engine-present-first-run.log,
menu-loading-gameplay.csv, menu-loading-gameplay.log, corresponding PNGs and
menu-loading-gameplay-summary.json. Do not commit dumps or game binaries.

## Boundary/reset and trace changes

Reset previously latched inReset forever. ResetSucceeded now explicitly exits
that state; failed Reset remains suppressed until a successful retry. Both Reset
and ResetEx have a regression test. The production boundary now seals replay
before the actual Present, while the DISCARD backbuffer is still valid. Scenes
generated inside presentation are excluded from the boundary state machine.
Replay eligibility uses actual draw commands and overflow state, not `rec >= 500`.

## OpenXR integration (2026-10-21)

- DLL `39A2EF2C5687B934A29BAB63CB5EABAD97706B093FC7612376F2ABAC33D1925A`,
  PID 31344: 4,048 completed Presents at detach, both replay eyes successful,
  session reached FOCUSED and submitted a projection layer (frame 3300).
  This session was user-operated after UI automation was stopped. Its log
  confirms operation, not visual stereo comfort. Saved lighting unexpectedly
  overrode the requested StereoMode=2; this was corrected next.
- DLL `6BF3F250D1A1F3737052A05CF804ADD156723700808BD9AB9D7FF7C3D51A510B`,
  PID 43052: effective StereoMode=2 verified; menu -> Continue -> loaded barber
  level visually observed with OpenXR enabled. Over 5,000 completed Presents.
  Actual backbuffer = RT0 (`0D62C980`), implicit backbuffer `0D62CB60`,
  actual dimensions 3076x3140. Hence RT0 capture targets the presented surface.
  CaptureSerial requested gameplay dumps through the existing diagnostic INI.
  Pair `run_43052_frame_003` is 2676x2728 per eye, RGB MAE=0, maximum difference=0.
  These are separately rendered live/replay images, captured before debug eye
  duplication. This establishes zero-shift replay equivalence for this scene.
  It does not establish geometrical stereo or world scale. This session's runtime
  was SYNCHRONIZED (layers=0) during the automated capture, not visible in HMD.
- Native gameplay hooks remained disabled (StartupMode=2). Hands, aiming and
  native FOV changes were not validated by these tests.

CSV output now uses a fixed 8192-row queue and a separate process-lifetime worker.
The rendering thread never waits for the queue lock or writes to disk. Contention,
full queue and event-budget exhaustion increment dropped counts. The worker pins
the proxy module, which already installs process-lifetime hooks. Shutdown cannot
join under DllMain's loader lock; abrupt process exit can lose the last 500 ms
flush interval and queued rows. Counts and prefix captures must be interpreted
accordingly. Per-1000-call human-readable logs are still synchronous.

## Remaining gates

Independent usable ETW reference; actual game-matrix disparity, calibrated world
scale and correct render-camera/submission-pose matching; native gameplay hook
regressions; final HMD validation. No complete stereo fix is claimed.
