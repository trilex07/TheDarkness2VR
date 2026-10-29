# Replay buffer versions — 2026-10-29

Baseline: `06023419`, `codex/replay-correctness-cpu`.

Dynamic VB/IB objects previously retained COM identity, but replay read their final contents. The six actual-source cases now consume A then B in both eyes, including DISCARD and ordinary overwrites; the disjoint NOOVERWRITE control still passes.

Implementation records only writable Lock ranges at Unlock. Separate buffers receive those ranges in command order with the original offset, size, flags, format and usage. COM private data owns metadata and replica lifetime. Frame-start and state-block inherited bindings are remapped; live buffers, shaders, DirectRecordCopy and camera coverage are unchanged. Draw consumption and uploads share the existing recorder lock. No GPU readback, explicit GPU wait, blanket whole-buffer copy, or per-draw allocation was added.

Skipped/overflowed recordings advance replica contents before the next frame. Standard Reset drops replicas; ResetEx preserves them with native resources. Untracked/shared/alternate-interface resources, nested locks, failed uploads and in-recording static-buffer mutations reject the incomplete pair rather than publish wrong geometry. Overflow rejects that pair but preserves the next frame's baseline.

Bounds: 16 MiB of buffer payload per recording, 64 MiB of owned replica allocations, 8192 metadata objects (576 KiB on x86), 64 runtime interface tables. The existing shared record arena retains capacity; these are incremental logical bounds, not a bound on total driver VRAM or process memory.

Offline optimized x86 measurement: 256 writable ranges / 1 MiB per frame, 300 frames after warm-up, actual hooks and recorder with a CPU D3D9 double. Final run: 0.066 ms capture plus replay, 0.015 ms unhooked baseline, approximately 0.050 ms incremental work; zero steady-state allocations. Inherited binding scan: 0.069 us/apply in that double. These numbers exclude real D3D9 calls, GPU scheduling and driver Lock stalls; they are not headset FPS measurements.

Validation: both x86 configurations; buffer version acceptance; live record receipts; frame references; replay surface/state receipts; live state-block synchronization; state snapshots; UP geometry; tentacle scope; projection history; shader registers; native culling geometry/rebuild.

Reproduce: `test/build_replay_buffer_versions.ps1 -RequireConsistentReplay`. The test uses actual interception, recorder, binding/draw hooks, replay cases and recorder lock. It covers both index widths, inherited/multiple streams, zero-size/read-only/failed locks, owner release, Reset, worker uploads, resource/payload limits and overflow recovery. MAIN owns the single consolidated PSVR2 check. No Steam access or deployment occurred.
