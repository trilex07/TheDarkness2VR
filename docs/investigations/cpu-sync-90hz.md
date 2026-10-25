# CPU frame pacing: bounded next measurement

Target: 90 Hz requires every submitted frame to fit within 11.111 ms. The user
reported roughly 22 ms CPU and 30–40 FPS in fpsVR during a 2560×2614-per-eye
trial. That trial's matching detailed log is unavailable; it must not be
inferred from older sessions.

Existing `d3d9.log` from 2026-10-25 measured D3D9 live plus replay at about
7.6–9.7 ms in loaded gameplay at 2048×2091, with short windows below 90
submitted layers/s. The older 3076×3140 logs measured around 9–11 ms D3D9,
but many had no submitted XR layer and are not headset FPS evidence. These GPU
queries exclude D3D11 transfers and OpenXR. Existing `[StereoPerf]` covers
only the mod's eye-pair CPU wall segment. `[FrameContext] CPU-window` uses
`GetTickCount64`, so its 0/16/31 ms values are too coarse to diagnose a 22 ms
stall. The old PresentMon CSV belongs to a different build and scene.

The current shared-texture path calls `WaitSharedReader` and a flushing D3D9
event fence for each eye. `xrWaitFrame`, swapchain waits, `xrEndFrame` and the
original SwapChain Present can also block the render thread. Their durations
were not separated by the existing counters. Global GPU utilization does not
distinguish CPU execution from GPU or compositor waits.

`[Diagnostics] CpuSyncTiming=1` in a diagnostic build now logs QPC wall time,
sample count, maximum and threshold counts for those exact waits, the shared
capture, D3D11 eye draw and Present-boundary work. It is OFF by default and
absent from the ordinary build. It does not change rendering, wait semantics,
resolution or the installed hand candidate. The first future 5K gameplay run
can identify which stage crosses 11.111 ms before a targeted optimization is
chosen. No new Steam or PSVR2 performance result is claimed here.
