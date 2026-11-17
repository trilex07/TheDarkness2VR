

## 2026-11-17: optional movement-stick outer range

Source-only candidate adds [Input] MoveStickOuterDeadzonePercent (default 0)
and bounded [Diagnostics] MovementStickProbe (default off). Identical radial
math/test corpus is shared with the other two public Trilex mods. Reduced Pico
north amplitude is a hypothesis pending raw controller evidence; no root cause
or headset fix is claimed. This does not change the separate VDXR startup path.
Offline tests/build PASS; runtime NOT RUN. Activation, limits and validation:
docs/PICO-MOVEMENT-STICK-RANGE-20261117.md.


## Player AMD transfer timing scope (2026-11-17)

Full player d3d9 log: sustained world-bearing XR cadence 33.9-56.0/s in
55 fully world-bearing report windows (median43.5) at72Hz; no before/after
baseline. right+restore CPU wall 11-15ms in gameplay, transfer mainJoin
5-7ms per job and worker body ~0.1ms are OVERLAPPING, not additive. CPU
wall / D3D9 timestamp spans do not identify GPU Busy. Existing mainJoin
contains staged D3D9 producer drainage + D3D11 publication + pending finish.
[Diagnostics] StereoTransferTiming=0 default, 1 subdivides that existing
scope (mean/peak per job, 240-job cadence). It changes no waits/query flags,
thread ownership, pixels or render path. Offline x86 /O2 diagnostic build
PASS; runtime/remote AMD NOT RUN. This is instrumentation, not an FPS fix.
See handoffs/HANDOFF-player-amd-perf-20261117.md under docs/agent-workflow.
