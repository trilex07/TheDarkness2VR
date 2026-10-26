# Sampler caps cache candidate (offline)

Base: `010a84e`. Candidate branch: `codex/sampler-caps-cache`.

## Correction

`ResolveSamplerValue` queried `GetDeviceCaps` for every recorded/live
`D3DSAMP_MAXANISOTROPY` setter in SubmitMode 4. Existing outdoor sampled stacks
identify repeated caps queries; approximately 0.5 ms/frame for the caps subtree
is an upper bound, not an established achievable saving.

The resolver now keeps one successful positive MaxAnisotropy value per thread,
keyed by the non-owning device pointer and a global atomic generation. The first
statement in HookDevice, Reset and ResetEx invalidates that generation, including
HookDevice's already-hooked early return and a failed Reset. Failed/zero queries
are never cached. A query crossing invalidation cannot publish its result into
the new generation. Device changes evict the previous thread-local entry.

The previous min(cap, max(original, requested)) clamp is retained. The unchanged
sampler hook records that resolved value and returns the original setter HRESULT.
No COM ownership, asynchronous driver calls, permission caching, EXE writes,
new hooks, profile changes or new logging are introduced. ScopedDiffuseFilter's
separate capability checks are unchanged.

## Offline verification

- `./tools/stereo-math-tests/build-sampler-caps-cache.ps1`: 2,071 checks of the
  actual helper; source integration verifies resolver guard, exact sampler
  forwarding/recording and unconditional entry invalidation at all three sites.
- `./tools/stereo-math-tests/build-material-sampling.ps1`: 424 checks using the
  real D3D9 HAL; sampler restoration and stateblock behavior pass, no Steam/XR.
- `./build.ps1 -DiagnosticCapture`: PASS.
- `./build.ps1`: PASS.

Repeated query elimination is offline verified. Gameplay performance, stability
and PSVR2 acceptance are NOT RUN; no claim of 90 Hz or a dominant-bottleneck fix.
Next check: comparable controlled gameplay A/B of the caps subtree and frame
percentiles, without unrelated configuration changes. MAIN owns authorization.
Rollback is a revert of this isolated commit or the preserved baseline DLL; no
installed files were touched.
