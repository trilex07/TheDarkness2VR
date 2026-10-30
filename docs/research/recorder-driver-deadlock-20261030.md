# Recorder / native-driver deadlock correction

Base: MAIN recovery `1dd6205e` (includes `213bb4f7`). SIDE only; no deployment.

The old nonrecursive CAS/Sleep(0) recorder lock enclosed versioned native Draw,
Lock/Unlock and the entire replay. A driver callback reentering recording, or a
native draw waiting for a recorder worker, could never complete. The regression
compiles that revision's actual lock, hooks, buffer interception and replay
admission/cases; five bounded child probes reproduce those waits.

The correction uses an SRW mutex for CPU bookkeeping and releases all nesting
levels across native driver/COM calls. Native leases prevent stream teardown.
Replay owns an immutable command/payload phase instead of holding the mutex.
Reentrant or unordered overlapping calls reject the ambiguous stereo pair;
they never certify guessed geometry ordering. Exact source ranges arriving
inside a callback/replay/upload are retained in a deferred journal, forwarded
through native Unlock, and applied after native leases finish. A drain consumes
one batch; callbacks use the other arena. Next-frame geometry recovers.

Flags, offsets, inherited bindings, DISCARD/NOOVERWRITE, source ownership and
ResetEx replica preservation remain intact. Busy Reset/ResetEx returns
D3DERR_DEVICELOST before dependent resources are destroyed. Existing per-command
SEH gates release this thread's abandoned bookkeeping/leases; interrupted
replicas remain ineligible. Legacy payload and state-block history entry points
cannot mutate replay-owned storage. No fences, readbacks or spin waits added.

Deferred payload capacity is capped at 16 MiB per arena; each of the two
reusable descriptor arrays is capped at 65,536 records. Normal recorded payload
and replica budgets retain their existing 16/64 MiB limits. No new INI settings;
ReplayBufferVersions=0 and ReplayStateFilter=0 remain quarantined.

## Validation

- `test/build_replay_buffer_versions.ps1 -BaselineDeadlocks`: five original
  deadlocks reproduced (draw, worker, Lock, Unlock, replay), bounded 1.5 s children.
- `test/build_replay_buffer_versions.ps1 -RequireConsistentReplay`: six original
  VB/IB failures, NOOVERWRITE, offsets/inheritance, ownership, Reset, quotas,
  ordered and concurrent workers, eleven callback/recovery probes, native SEH,
  unsupported buffers, disabled path PASS.
- Same fixture, `-BaselineCpu`: 0.064 ms/frame before versus 0.082 ms/frame after
  for 1 MiB / 256 writes + mock replay, 300 frames; +0.018 ms measured overhead.
  Both have zero steady allocations. Callback recovery: about 0.33 us/frame,
  zero steady allocations over 1,000 frames, two 4-byte retained payload arenas.
  CPU fixture only, excludes actual driver/GPU stalls; no FPS claim.

Other relevant checks: production/diagnostic x86 builds, live API results, UP
geometry, COM reference ledger, live state-block synchronization, projection
history, stereo state/surface receipts, tentacle head scopes, constant replay
window and direct record copy. See the individual build scripts for repeatable
commands. Existing snapshot-receipt suite was not rerun after its earlier OS
execution block. Steam, gameplay, headset visuals and runtime CPU validation are
NOT RUN; MAIN owns consolidated PSVR2 verification. Source-only publication.
