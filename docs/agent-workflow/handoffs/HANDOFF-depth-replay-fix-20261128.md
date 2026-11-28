# Depth capture replay restoration

- **Date** 2026-11-28
- **Task ID** depth-replay-fix-20261128
- **Role** MAIN
- **Branch** `codex/darkness2-depth-replay-fix-20261128`
- **Base** `276c757c`, paired composite depth capture

Both eyes copied real INTZ depth in frame 3990. Those draws returned
success, yet stereo replay rejected the frame before right-eye submission.
Paired shadow frame 10759 produced the same failure. The same process
completed the ordinary left-eye-only depth capture in frame 13634.

`DeviceSnapshot::Apply` restores the render targets via hooked setters.
During right-eye replay the render-target hook called `NativeMutation`
even inside the exact-device `InternalRestore` scope, setting the recorder
conflict bit. The render-target and depth hooks now forward those specific
restores directly. The snapshot copy detaches depth through the original
setter. Ordinary state-block recording still takes the normal hook path.

## Live verification

- First candidate `D378BEE7` completed full OpenXR and native Present in
  frames 2990, 3105 and 4906. The 3105 shadow audit matched 22,847
  reprojected points, with median mask error 0.00157.
- Narrowed candidate SHA-256
  `F7DD2B35EF8EE7C980C4321BE78829F0FB6C4E6D5C316BAD6787032AEBDED241`.
  Win32 build, 101 Python tests and 12,019 native state-block fixture
  checks passed.
- The game accepted the actual Steam campaign checkpoint copied into a
  private profile. Narrowed-candidate frames 7133 (paired fog depth) and
  7251 (paired shadows) completed and validated. Shadow frame 7251 matched
  21,710 depth-consistent points with median mask error 0.00155 and p99
  0.0182.
- All compared renders used the crane yard checkpoint. The Villa lighting
  and shadows still require their own gameplay captures.
