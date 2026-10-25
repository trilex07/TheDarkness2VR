# Isolated engine Present candidate — 2026-10-25

Based on `638344f`, branch `codex/diagnostic-engine-present`.
Decision: **GO for the controlled mono trial after explicit user approval**.
Deployment/live test: **NOT RUN, awaiting approval**. This is not a claim that
the candidate is already safe under the current live overlay or headset.

## Evidence and explicit exception

The approved static path is documented in verified-engine-present.md and
actual-swapchain-observer-20261025.md. The historical call-site mechanism
completed over 63,000 calls; a separate trace contained 67,472 rows, including
occluded/mode-changed results which were not displayed-frame successes.
The failed COM shadow is not reused.

Current original EXE freshly audited read-only: SHA-256
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`,
PE32/I386, timestamp 4F68A875, preferred base 00400000, image DEC000;
14 sites and 18 ranges match. The existing acquisition audit also pins the
whole dispatch range. At image+694052 / preferred VA A94052:
`8B 41 0C FF D0` = MOV EAX,[ECX+0C]; CALL EAX.
Six stdcall arguments were already pushed; return is image+694057.

**This candidate changes five executable instruction bytes in process memory**
to CALL rel32 EngineSwapChainPresent. This is the explicitly requested exception
to the previous no-EXE-patching diagnostic rule. It does not write the EXE on
disk, change a COM vtable, truncate private methods, patch a D3D implementation,
or install an alternative wrapper object. No approval to deploy was inferred
from the request to prepare it offline.

## Candidate behavior

Only compiled with `build.ps1 -DiagnosticCapture`; runtime OFF by default:

```ini
[Diagnostics]
EnginePresentCallsite=1
```

No standard INI change. Other native gameplay/FOV/IAT installers stay suppressed.
The opt-in installer runs in the first successful CreateDevice callback, before
the proxy returns that device to the engine. It never patches from EndScene or
Reset; EndScene cannot become a capture witness. The existing production
installer is unchanged.

Full on-disk EXE SHA is checked against two explicitly audited originals (D46B
above and prior 55C8), with PE identity and the five original loaded bytes.
The file is held with read-only sharing while auditing/installing, preventing a
concurrent file replacement. Unknown identities or altered site bytes refuse.
The DLL is pinned before installing a process-lifetime call target.

The diagnostic branch of the existing EngineSwapChainPresent resolves slot 3
from the actual call argument once, then forwards the six unchanged arguments
exactly once. Ordinary HRESULTs, including failure and occlusion, are returned
unchanged. GetDevice's temporary COM reference is balanced; the actual pointer,
vtable, device, slot owner module, caller and thread are logged, followed by
entry/forward/return/recursion counts.

A thread-local recursion guard covers the entire callback, including GetDevice
and forwarding. Reentry or a null/self slot is rejected with D3DERR_INVALIDCALL,
not recursively forwarded. A detected cycle latches capture failure; this
exceptional path intentionally reports failure rather than promising a display.
It does not claim to eliminate every possible cross-thread overlay behavior.

Only an outer call from the expected return address, with a valid owning device
and no reset/fault, enters the existing SwapChainPresent boundary machine.
It sets the current caller before ApplyBoundaryDecision; replay/submission run
before DISCARD Present. PresentReturned runs after the original function returns;
COMPLETE requires the existing exact-frame witness and D3D_OK. The next-frame
arming and pending-publication schedule is retained. A recursive failure cannot
be promoted to COMPLETE even if the enclosing overlay swallows its error.

## Risks and rollback limits

- The five-byte write is not atomic. Its safety depends on installation before
  the first device is returned to the engine, not on thread suspension. Do not
  add late installation, hot reload, or asynchronous unpatching.
- Current overlay interaction is untested. Recursion rejection prevents an
  unbounded same-thread cycle but is a failed trial, not a successful Present.
- The non-Ex fallback at A94072 is not patched. If the run uses it instead,
  zero callbacks means NO-GO; do not infer success from EndScene counts.
- Patch/protection/cache failures attempt immediate original-byte rollback.
  An uncertain rollback fails fast rather than returning a half-patched process
  to the engine. Protection failure can therefore terminate this diagnostic run.
- No live unpatch while rendering. The synthetic rollback API is tested on a
  private memory page. Real rollback is process exit plus restoring DLL/INIs;
  the original EXE file never changes and process memory disappears on exit.
- Historical success does not prove current runtime or PSVR2 success. Existing
  baseline COM hooks remain; this change isolates only the new engine observer.

## Offline results and provenance

- New x86 test PASS: executes the original five-byte instruction shape on a
  synthetic private page, patches CALL rel32 to a stdcall wrapper, verifies all
  six arguments, one original invocation and exact success/failure/occluded HR.
  Executes the restored original sequence afterward. Tests recursive forwarding,
  identity/byte rejection, duplicate installation, protection/cache rollback
  failures, refusal to overwrite a foreign replacement, and two-phase witness.
- Frame-boundary simulator: 2,327,551 assertions, zero failures.
- Matched capture suite: 15/15 PASS, including incomplete/failed publication.
- PE/dispatch audits: 13/13 PASS.
- Production and diagnostic x86 builds PASS. Normalized production preprocessing
  is identical to the preserved baseline, SHA-256
  `a9bcfd7a4e2f507716bc777e0f5918c16d4155728b039ea5583e8b4dcfa36cd9`.
- Built candidate DLL SHA-256:
  `89FD7DFC4967894783F136D23AF687BDBA8D0F6BD0B06FA4FAE8655CE5B4BFE0`.
  Binary remains local; no binaries, private logs, or game assets committed.

Installed DLL/INIs remain the prior restored baseline (9C96DC85..., C802439A...,
586F536A...). No game launch or deployment in this task.

After explicit approval: re-audit identity; ensure no other session is active;
back up/hash DLL and both INIs; enable only this opt-in with SubmitMode=4,
StereoMode=0, DisableOpenXR=1, MatchedFrameCapture=0. Stop/restore on any failure.
Require nonzero actual Present entries, equal forwards/returns, observed HRESULTs
and zero recursion/errors. Only after PASS may a separate loaded-gameplay
SubmitMode=4/StereoMode=1 capture run proceed. Restore and hash-check the original
deployment after testing. No headset acceptance without the user's physical test.
