# Captured producer-flush compiler dependency

The authorized retry 20261030T155718Z-497ee850 completed with zero ETW and
PresentMon event loss, unchanged DLL/configuration hashes and matching frozen
DLL/PDB/MAP. The command history was decoded before MAIN stopped PID35484 with
the user's permission. The earlier 154731Z attempt retains its incomplete flag: it
has ETW/PresentMon but no history because the game was closed before export.

## Exact dependency

History sequence1544314, frame12020, ownerTID39032, generation1:
QPC315729720772..315729933347 (10MHz),21.2575ms. Operation producerFlush, eye0,
slot1, HRESULT S_FALSE. The existing path is VrFrame -> RenderEyePairFromReplay
-> StartStereoTransfer -> CaptureBackbufferToShared -> producerQuery.GetData
(nullptr,0,D3DGETDATA_FLUSH). No new fence was introduced.

The renderer switches out at ETW22589980us and back in22611195us:21.215ms off.
ReadyThread22611187us names releasing TID38200. Its stack includes
ReleaseSemaphore -> nvd3dum+absolute5C83A8EC ->5C846F57 ->5CB633DC. During the
exact API interval,19/21 samples of TID38200 include nvgpucomp32. This is a
dependency proved by the wake relation, not just adjacent timestamps. The
switch-out user stack is absent; the exact API interval comes from the history.
Full ready stacks and absolute PCs remain in the JSON and retained ETL.

## What the preceding commands establish

The32768 retained records span frames11996..12020 and479.2643ms. They contain
28938 indexed draws,1907 DrawUP,1010 Draw,525 clears,194 surface updates,142
blits,48 producerFlush operations and4 CreateTexture calls. No CreateVS/PS is
in this retained window; earlier creations and overwritten records are unknown.

All texture calls return through nativeBC9B5F. Two occur227.7ms before the
flush, two65.7ms before it. The longest CreateTexture is0.3074ms. This capture
therefore does not reproduce a long CreateTexture call; the long boundary is
the later producer flush. It does not prove which earlier operation caused
compilation or whether the driver work was a first-use pipeline variant.

At the flush the observed VS/PS IDs are167FC020/1608F9A0. These are effective
bindings at the wait, not compiler shader attribution: previously queued draws
may own the dependency. Pointer IDs are not hashes. No shader hash is guessed.

## Decision and limits

No safe whole-ms correction is identified. Do not remove the required FLUSH,
move it across the multithreaded driver lock, cache unrelated texture owners,
or repeat speculative shader prewarming. The existing native ownership and
fence investigations remain closed. Exact compiler input is still unknown.

1343 combat application intervals average18.6079ms; p9522.4956ms, max43.4283ms.
GPU Busy averages6.0855ms. These are profiled application timings, not PSVR2
FPS or proof of uninstrumented performance. Diagnostic state reads accumulated
224.9226ms before the ring froze; wrapper/ETW overhead is additional and cannot
be subtracted blindly. The source contains the accepted performance changes.
No renderer correction, FPS improvement or visual-fix claim follows from this
capture. Runtime stereo/visual acceptance remains with the user.

The DLL/INI and rollback package were left unchanged. Offline build/regression
results belong to5ff1b7dd; this evidence-only commit does not require relinking
or replacing the exact captured symbols.
