# Replay CPU reductions — 2026-10-30

Base: verified committed MAIN renderer d3823d7a plus completed VB/IB correctness 2d5bf4b4. Isolated branch codex/render-cpu-optimization; MAIN worktree untouched.

1. Replay-local scalar state filter: repeated successful public render, sampler and texture-stage writes are skipped. Knowledge starts empty each replay; state blocks, exceptions, vendor/unknown commands invalidate it. Draw scopes invalidate their sampler/scissor entries. Failed writes never establish knowledge. No retained resource pointers, allocations, queries or synchronization are introduced.
2. Adjacent constant batching can borrow a durable recorder-arena packet until its barrier. Singleton packets avoid the assembly copy; a second adjacent packet materializes the first. Transformed stack packets retain owned copying. This benefits the existing ReplayConstantBatch path when enabled; its configuration and capture gates remain unchanged.

Offline x86: 4,000 patterned material draws matched full state at every draw, with scalar driver calls reduced 128,000 to 7,379 (94.2%). This is a synthetic repeated-state workload, not a measured game redundancy ratio. 500,000 singleton 68-vector packets: copy 7.430 ms versus arena borrowing 0.787 ms; 544 MB of assembly copying eliminated. These timings do not include real driver/GPU costs and cannot establish game FPS gains.

Actual-source suites cover replay receipts, buffer versions/NOOVERWRITE/Reset/concurrency, both constant-loop build flavors, 360,000 batch operations, projection carry/partial uploads and tentacle scopes. Both x86 DLL configurations pass. Extraction fixtures now accept the existing buffer-correctness baseline. The capability cache was examined and already has an explicit A/B switch; its default remains unchanged.

Historical ETW evidence (2026-10-26) attributes outdoor growth to native preparation, recorder/syscalls and driver work. It predates this baseline. No new current-runtime bottleneck distribution or 90-FPS claim is available. Native traversal, actual repeated-state frequency, driver Lock/shader costs, GPU frame budget at 5K and transfer synchronization still need MAIN's current capture. No Steam access, backend replacement, native EXE patch, deployment or merge.

Reference technique reviewed: DXVK state tracking and bounded uploads (https://github.com/doitsujin/dxvk); implementation here is original and scoped to the existing replay contract.
