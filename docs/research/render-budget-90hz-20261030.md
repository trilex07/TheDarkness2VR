# CPU follow-up after the stability candidate

Base: deployed MAIN candidate 69b02db1, on isolated codex/render-budget-90hz.
User reports outdoor street combat drops toward 60 FPS, roughly 20 ms CPU and
roughly 1 ms GPU. These are user estimates, not an ETW RUN/wait decomposition.
90 Hz needs an 11.11 ms frame budget. No stable-90 or runtime saving is claimed.

The existing retained detailed outdoor captures predate this DLL. They identify
native preparation, recorder/WOW64 transitions and driver work, but cannot
assign the remaining time in this session. The existing caps cache, binding
filter, constant batching, owned-copy and worker candidates were reviewed;
no quarantined feature is enabled and no speculative native traversal is skipped.

Implemented two arithmetic reductions shared by live/replayed projection:
- SIMD row composition keeps the scalar +0 then k=0..3 multiplication/addition
  order, unaligned accesses and alias-safe output. It uses ordinary SSE, no FMA,
  reciprocal approximations, new synchronization or retained frame state.
- Reuse the projection coefficients already computed by the stereo wrapper,
  rather than estimating them again. A known camera needs no matrix row norms.

Optimized x86 CPU test: scalar 13.48 ns versus SIMD 6.20 ns per composition,
0.270 versus 0.124 ms for 20,000 compositions. This is only 0.146 ms in that
synthetic workload, not enough to close the reported frame-budget gap. Never
convert this number into measured headset FPS. The test compares the production
function against precise scalar arithmetic from MAIN, 100,000 finite matrix
pairs with output alias/partial overlap, and 80,000 full float bit-pattern pairs
under all four MXCSR rounding modes. Non-NaN outputs match bits; NaN outputs
retain their classification. The initial-zero load preserves underflowed signed
zero behavior found by the stronger regression.

Validation commands:
- test/build_matrix_cpu.ps1
- test/build_projection_history.ps1
- tools/stereo-math-tests/build-protected-head.ps1
- tools/stereo-math-tests/build-rendered-fov.ps1
- tools/frame-boundary-simulator/build-replay-constant-window.ps1
- test/build_replay_buffer_versions.ps1 -RequireConsistentReplay
- test/build_tentacle_head_scope.ps1
- build.ps1 and build.ps1 -DiagnosticCapture

For the next substantive engine change, MAIN needs a current street-combat
capture using the existing tools/perf/Capture-DarknessCombat.ps1 collector,
matching DLL/INI identities and focus-active gameplay. Separate render-thread
RUN, BLOCKED/READY, native preparation, recorder/syscalls, driver and XR spans.
Do not add overlapping timing categories, lower FOV, omit rooms/shadows or
remove synchronization to manufacture 90 FPS. No new profiler was introduced;
no game launch, Steam access, MAIN edits, deployment or merge by SIDE.
