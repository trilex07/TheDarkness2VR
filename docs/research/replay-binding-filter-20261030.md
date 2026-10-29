# Replay binding filtering - offline validation

Base: latest committed MAIN 7371fbe6 plus SIDE CPU reductions 18006227.

Texture, VS/PS, declaration, stream (including offset/stride), instancing frequency and index bindings previously entered the driver on every recorded setter. Replay now filters exact successful repeats inside one replay only. Shader metadata selection runs only on an actual successful shader change. The existing command/snapshot owners retain identities. Cache size is fixed; no extra resource retention, allocations, getters or synchronization.

State blocks and caught driver exceptions clear all binding knowledge. FVF invalidates declarations; UP draws invalidate stream zero; indexed UP also invalidates indices. Vendor/unsupported render state commands conservatively clear binding knowledge. Every VB/IB packet still resolves and validates its buffer version before filtering. Native calls, payload copies, snapshots, FOV/tracking, draws and replay ordering remain intact.

Actual-source x86 regression compares every binding at 4,000 simulated draws, including SB/FVF/UP side effects, instancing, offsets and vertex textures: 64,320 to 6,919 binding calls (-89.2%), 8,218 to 436 metadata lookups (-94.7%). CPU-only mock 0.233 to 0.209 ms/frame; real driver/GPU excluded. Also checks failed setters, retry, nulls, unknown slots, new replay reset and invalid buffers despite identical handles. This synthetic material workload does not establish the real game's repetition rate or FPS gain.

No Steam deployment or runtime performance claim. MAIN owns PSVR2 acceptance. Existing historical ETW predates current native FOV and buffer versioning; shader creation, traversal and transfer waits remain current-runtime measurement gaps.
