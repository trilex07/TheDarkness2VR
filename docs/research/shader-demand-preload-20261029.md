# Demand-sized one-use shader inventory

The retained `%LOCALAPPDATA%/Darkness2VR/d3d9.log` reports 1,362 startup
objects for 681 bytecodes (two per code), then 23 depleted misses and two
unknown misses at 490 requests. There were still 897 unused objects. This
demonstrates uneven demand; it does not time the misses or establish this as
the source of sustained outdoor frame time. The log does not establish the
exact DLL SHA of that session.

`Pool` now counts successful transfers and successful fallback creations per
exact bytecode, saturating at eight per process. Failed requests do not grow
the quota. The persisted quota is the maximum of the prior quota, configured
minimum (one or two), and observed demand. It is a bounded high-water mark,
not a concurrent-use estimate. Repeated level loads can saturate it; later
requests still use the native API.

Startup covers all programs once, then adds successive copies up to their
quotas. Creation stays synchronous on the owning device thread, stops after
driver failure, and retains the ten-second between-call budget. The global
inventory ceiling is 2,048 objects. No live refill, binding, shared COM-object
reuse, driver cache manipulation, EXE writes or asynchronous D3D9 calls.
Transferred objects belong exclusively to the game; Reset handling is unchanged.

The versioned DSW2 archive has a bounded quota before each word count and full
bytecode. Validation admits the entire archive before any driver work. A new
`shader-demand.bin` preserves the old `shader-preload.bin` for rollback.
When no valid demand archive exists, startup reads the old archive, then the
existing seed. Disk persistence uses the existing snapshot/background writer;
that writer never accesses D3D9 or engine memory.

Comparison: [DXVK's D3D9 shader implementation](https://github.com/doitsujin/dxvk/blob/master/src/d3d9/d3d9_shader.h)
separates per-object D3D9 shader interfaces from shared backend shader modules.
Our proxy does not own the backend and cannot transplant that sharing scheme.
This change instead extends the existing independently created one-use inventory;
no DXVK code was copied.

Validation: x86 actual-pool tests cover successful demand accounting,
disabled/wrong-device/wrong-stage refusal, saturation, independent objects,
ownership transfer, quota roundtrip, malformed/truncated input, and migration
of the retained private 683-program archive. Existing startup and actual-source
stereo regressions and both full x86 configurations must pass before deployment.

Runtime validation remains outstanding. The first session learns demand; the
next startup can use it. Unknown shaders require successful native creation
before learning. No reduction in milliseconds or FPS improvement is claimed.
