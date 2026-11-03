# Full same-frame vertex-packet worker

User requested a materially larger offload after the tiny-matrix experiment
failed its benefit gate. MAIN implementation on
`codex/engine-parallel-work-20261103`, parent `ec5d6875`.

## Implemented boundary

`FullPacketPrepareWorker=1` prepares entire VS uploads on a persistent CPU
worker: raw device register history, WVP/STP references, overlapping partial
matrix reconstruction, fallback classification, sprite/FOV corrections,
screen-projection bias, previous projection and geometric head transforms.
The original serial transform remains the independent correctness oracle.

The owner snapshots only constant payloads and value metadata after recorder
seal. No borrowed engine memory, COM handles, D3D calls or global mutations
reach the worker. The worker publishes groups of16 completed packets with
release/acquire; the consumer uses a direct record index. An unfinished packet
immediately uses the original serial path. Projection publication and shift
diagnostics happen on the owner, at the original upload. Raw owner history is
still updated for exact fallback, so this experiment does not remove that cost.

Shadow/light selection, HRESULT handling, upload batching, eyeNext history,
tentacle draw scopes, transfers and all D3D9 calls retain their existing owner
and order. Any earlier failed device call disables speculative consumption for
the remaining replay. StateBlock clears worker raw history and makes shader
metadata unknown until an explicit recorded bind. Consumer checks current
shader semantics, FP controls, configuration and relevant native UI scales.

Maximum8192 packets,8MiB of copied float input,50000 recorder commands. Invalid
ranges, unsupported/unknown metadata, unmasked FP exceptions, capture mode or
insufficient work leave the original path. Cancellation and queue completion
precede recorder release, buffer reuse and the next frame. Output/input capacity
is reused; no pixel dimensions or D3DCREATE_MULTITHREADED changes are made.

Repository and existing Steam defaults remain OFF. This is an experiment,
not an accepted performance fix. When requested together with the old tiny
worker, full-packet mode takes precedence; the old worker is not also launched.

## Measurements and practical ceiling

The existing latest log has roughly1500–1880 VS records/frame in its final
120-frame windows (e.g.225581/120). These are aggregate counts, not complete
payload traces. The synthetic cases below exercise the actual transform code
with mixed uploads; they are not a reconstructed combat frame.

Private isolated x86 `/O2` run, after build activity ended:

- 2048 packets: same pure full-packet algorithm serial0.190ms;
  worker+submission+immediate join0.195ms, with input already assembled.
- Actual production owner snapshot+submission0.072ms and ready-consumer
  checks0.049ms for2048 packets; warmed33-frame fixture, first excluded.
- 8192 stress packets: serial1.061ms, worker+submission+join1.139ms;
  actual owner snapshot0.289ms and ready consumption0.195ms.

Another run gave2048 serial0.193ms and8192 serial0.883ms. Concurrent compilation
made earlier runs much slower; do not compare those as optimization results.
Preparation and consumption fixtures use17-row known uploads, whereas the
transform fixture mixes partial, unknown, UI,68-row and StateBlock cases.
These are separate workloads: subtracting their timings is **not** a measured
speedup. Owner raw-history writes, late fallbacks, cache contention and final
join further limit any gain. The realistic-sized computation is already below
0.2ms in that run (later runs0.19–0.26ms). A large sustained combat FPS increase
is not supported by this data.

Runtime logging `[ReplayPackets]` records hits, fallback, copied bytes,
preparation and join averages. Only controlled gameplay could establish useful
overlap and the total frame-time result. No fresh capture, launch, install or
headset validation was performed in this task.

## Larger native-threading investigation

Exact EXE SHA256
`D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
Private Ghidra exports are outside Git in TEMP, with SHA headers and COMPLETE
markers. No executable patch or native hook was introduced.

- Pool constructor`0xBD3D10` calls`0x741EE0` (GetSystemInfo) at`0xBD3E1A`,
  subtracts one and clamps to at least one worker. Thread entry`0x906E40`
  bytes`51 8B 09 E8 58 B4 1E 00 C3` calls`0xAF22A0`.
- `0xAF22A0` dequeues completion-port jobs and invokes`0x9AA4D0`. Existing
  capture`20261030T221816Z-61b77f88` already has many engine worker threads;
 3934 non-main EXE stack samples include dispatcher return`0x9AA573`.
  This disproves the assumption that the engine simply lacks worker threads.
- `mGraphicsWorkerType` describes a resource/class field. Reflection constructor
 `0xEC3F60`, bytes`68 00 15 0E 01 68 B0 92 F3 00`, registers GraphicsWorker;
 `0x44DA20` loads/validates that resource and installs owner references. No
  evidence supports treating this field as a safe thread-count toggle.
- Hot PC`0x992311` belongs to`0x992210`, a mutating allocation/tree operation;
  hot PC`0xA41112` belongs to`0xA40FF0`, which mutates retained references and
  child storage. Neither is an isolated immutable math task that can simply
  be scheduled elsewhere. The previously closed resource-loop investigation
  was not reopened as a speculative patch.

## Validation

- Full protected-native diagnostic x86 DLL build PASS, no new warning.
- Actual serial transform versus threaded full-packet output: bitwise PASS
  across four rounding modes, nine lighting presets, head transforms, named/
  fallback projections, partial device registers, screen bias, UI, NaN and
  negative-zero payloads. Actual snapshot/lifecycle and guards included:
  **942908 production /942909 diagnostic checks PASS**, including a changed
  native UI scale rejecting an otherwise ready packet.
- Actual replay consumer, prepared and fallback mixed: production and diagnostic
  each PASS384 streams /2304384 records /224468 checks. Existing float/binding
  order, carry, failure, null bind, caps and SEH contracts remain covered.
- Previous matrix worker65589 and cache8405 checks PASS.
- Protected head117021 and actual47/130-bone cinematic pack/restore PASS.
- Five-mode offline installer, package-selected default, exact rollback,
  active-game and corrupt-payload rejection PASS.
- Steam DLL remains`B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2`.

Private logs: `build/packet-*.log`. Tests use mod source and synthetic owned
data; native disassembly and game-derived captures remain private/untracked.

## Candidate

Preparation adds an isolated `Packets` profile with cache0, tiny worker0,
full-packet worker1. `Off` selects all three0. Existing Cache/Worker/Both controls
remain available. The manifest selects the frozen default; older manifests
retain Cache fallback. Every other setting comes from the exact installed INI.
Preparing/verifying the package does not install or launch it. A later accepted
runtime test is required before merge or default admission. Given the small
offline ceiling, do not promise a major FPS improvement or call this a completed
native AI/animation/render submission parallelization.
