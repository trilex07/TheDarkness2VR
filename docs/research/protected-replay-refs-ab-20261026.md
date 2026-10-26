# Protected replay references — authorized A/B, 2026-10-26

Exact runtime candidate: `c86c2c8`, diagnostic SHA-256
`524777e9d12f4b6fd3f09cbde74d86d77a9c4124395eccd6863657fdff911b74`.
The DLL and diagnostic INI are identical in A/B. Effective render INIs differ
only in UniqueReplayResourceRefs=0/1. Controls, geometric head, gestures and
1920x1071 actual shared-source size are retained. The later last-consumer fence
and quality changes are excluded.

## Actual result

The user reports **30–50 FPS in both A and B** outdoors. No HMD performance
improvement is accepted. The hypothesis that repeated COM references explain
the large reported outdoor slowdown is not supported by this test. Mode remains
OFF by default; reduced reference counts are not a fixed-FPS claim.

Both processes exited normally (Steam exit0): A PID33904, B PID40828. Last
sampled Present totals were16000 and25000, respectively, with entries,
forwarded, returned and successful counts equal, recursion/errors/statuses/fault
zero. Detach totals16130/25872 exceed those sampled receipts; unsampled final
Present returns are not individually certified. Neither log records C0000005.
This is two short surviving sessions, not long-term stability acceptance.

Actual reference retention changes occur: e.g. a B sample requested1685 and
retained721 exact pointers, while A mode retains every requested reference.
These are different sampled frames, not matched geometry or a CPU-time ratio.

## Limits of the comparison

A's intended35-second window remained focused with sampled layers1. Its
stop receipt was written at56.826s; analysis clips to the nominal first35s.
B's automatic recording lasted35.022s, but focus was lost approximately
10.72–12.06s after its start (bounded by adjacent sparse QPC markers). Subsequent
layers0 intervals are excluded; their approximately90 boundary/s cadence is
**not90FPS in VR**. An early B ref5/UI-only sample further prevents a comparable
25-second combat-window numerical A/B. No extra attempt was launched.

The existing logs provide five-second API-wall-time accumulators and sparse
QPC/FrameContext receipts, not per-frame execution, Ready/blocked stacks or
complete p50/p95/p99 distributions. No execution/wait decomposition is invented.
In fully enclosed focused A windows, original Present means0.341–0.369ms and
boundary-work means2.571–2.981ms; shared-reader means0.217–0.263ms/read and
producer means0.385–0.486ms/read. Scopes overlap and are not added together.
These scopes do not explain all reported20–30ms. D3D9 GPU timestamps remain
elapsed spans, not GPU Busy; no GPU-saturation claim follows from them.

## Preservation and next correction

Fresh original EXE SHA/PE/site/range audit passed. Backup covered DLL and all
four INIs. Originals were restored and all five SHA-256 hashes matched after
A and again after B. EXE disk SHA remained
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
Private logs/receipts remain outside Git.

Next focused correction: obsolete PassProbe performs GetRenderTarget/GetDesc/
Release on every live c0 upload until512 distinct shader/size keys occur.
A final log contains130 distinct entries (128 at the runtime snapshot), so this
cap did not stop the repeated diagnostic API work. It only feeds logging, not
projection or pass classification. Move it behind an explicit diagnostic opt-in;
preserve forwarding, matrices, validity checks and all rendering. Its duration
and FPS benefit remain unmeasured, so this is a new bounded candidate rather
than a claim that the dominant CPU cost has been found.
