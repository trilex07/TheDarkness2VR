# Hand release and image quality follow-up (2026-10-25)

The user confirmed visible hands move in PSVR2. The left hand appeared stuck
after bringing both hands together; pickup and throw direction still felt wrong,
and the image looked low resolution. The candidate below has not been retested
in the headset.

## Evidence from the completed gameplay session

- `HandSupport` entered two-handed at log line 1956 and returned to free left at
  line 2000. Native grab became Busy at line 1958 and remained Busy until line
  2243. The guarded native skin packer skips all human IK during Busy, so the
  free-left state could not reach the visible palette for that interval.
- Several ordinary prop throws reached the verified velocity hook. Its logged
  direction used `ThrowAimAssist=75`, allowing controller aim to dominate the
  powered hand stroke. Requests 5 and 6 did not reach the velocity hook; their
  cause remains unresolved. Enemy/weapon holds remain excluded from the override.
- The requested backbuffer was 3076x3140, but the gameplay RT0 and shared
  capture were 1680x1050. The diagnostic build reported
  `ProductionResolution=0`, so its existing GetClientRect path did not resize
  native scene targets. The OpenXR eye swapchain was 2676x2728.
- The heavy gameplay interval had D3D9 live+replay GPU means of 12-14 ms at
  that 1680x1050 source, already beyond the 11.111 ms budget for 90 Hz.
  D3D9/D3D11 fences and `xrWaitSwapchainImage` averaged below 1 ms. No causal
  claim about game-thread CPU time follows from these stage timings.

## Small correction and next bounded test

- Two-hand pistol support becomes ineligible immediately on a left grip or
  native grab, before the LB grab pulse. IK remains suppressed during Busy to
  avoid reintroducing the reported enemy-grab crash.
- The default throw assist and this candidate's INI are 20%, so the powered
  left-hand stroke dominates eligible ordinary-prop throws. Pickup ray
  alignment and skipped throw branches still require physical verification.
- The next diagnostic profile can opt into the already implemented native
  resolution hook at 2048x2091 and enable existing GPU live/replay split timing.
  Verify actual RT0, final eye dimensions and fpsVR before raising resolution.
  Do not claim a 5K/90 Hz result from this offline preparation.
