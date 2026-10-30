# Native submission work: ownership prerequisite (2026-10-30)

Current MAIN contains the reviewed release-batch correction 2f24b31e. It remains offline; Steam retains accepted ConsumedRecordCopy source 4a26aabd and its verified full rollback. No game launch, measurement or deployment was performed for this follow-up.

## Quantified target and limits

Latest complete retained capture: 20261030T205050Z-eab0975b; render TID 39476; 1,586 combat application intervals. A disjoint classification assigns each render-thread sample once to its first D3D9 native setter/runtime setter ancestor. Total 3,914 sample weight, approximately 2.468 ms/frame. This includes hook bookkeeping, recorder work and driver execution; it is an upper exposure bound, NOT a claimed recoverable 2.468 ms. Scalar-state-only calls are a small subset. Replay-local filtering alone cannot be advertised as removing all live/native cost.

Native EXE leaves remain approximately 4.05 ms/frame. The highest inclusive native addresses include 992311, 71F71F, 5222DA, 6F038D, A41112, 524C66 and 5C200C. Existing evidence identifies 71F71F as the material-builder upload caller and the latter UI/buffer chains in the completed UI replay investigation. Flat PCs in the closed AC36F0/BB79A0/C5D850 resource cycle are excluded from further optimization work. No separate, safely removable multi-ms native producer is established by this evidence. Do not invent one from inclusive sample totals.

## Implemented prerequisite

src/submission_epoch.h implements a fail-closed ownership gate for future native submission elimination. It records no graphics state, allocates no memory and makes no Windows calls. Successful write tickets become invalid on recursive/raw state mutations. A scoped bypass invalidates both edges and excludes cache reuse throughout nested raw replay/restore operations. Foreign-thread mutations, opaque native StateBlocks, exposed untracked interfaces and replacement devices permanently deny reuse for that device lifetime. Reset/configure cannot silently revoke those denials: existing StateBlocks/interfaces can outlive Reset. Actual runtime thread IDs must be supplied by the integration.

This header is deliberately NOT wired into native setters yet. A gate alone is not a complete cache or an optimization, and has no runtime gain. Correct integration still needs all raw mutation boundaries, actual StateBlock creation/alias escape interception, failure/HRESULT handling, and owned final eye-adjusted constant payloads. Never enable filtering based solely on a capture that happened to show one render thread, or on a guessed StateBlock vtable layout. Current StateBlockHook remains OFF.

Verification: x86 /O2 /W4 /WX regression PASS for late publication rejection, real second-thread mutation, nested raw-state bypass, persistent StateBlock/interface/device denial across reset/configure and invalid thread identity. Existing renderer builds at 2f24b31e remain the frozen offline artifacts; adding this currently unlinked header does not certify a native submission filter. No new profiler, capture, RPM/VB-copy/cache investigation or speculative executable patch.
