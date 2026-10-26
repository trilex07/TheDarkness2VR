# D3D9 execution backend: SYSTEM 9On12 prototype

Status: IMPLEMENTED / OFFLINE VERIFIED. Game, overlay, OpenXR and PSVR2 acceptance
NOT RUN. Performance saving NOT VERIFIED. No Steam start, deployment, package or
merge. Dedicated branch `codex/d3d9-execution-backend` starts at verified remote
`63b04ea902f9ee9ea815289457f43f29235ace63`; other worktrees and installed backups
are preserved. Native engine traversal remains Astra's separate subsystem.

## Evidence and scope of the decision

Use the retained COMPLETE zero-loss outdoor B, source804d0e7, PID43664/render
TID28400: `cpu-candidate-log-ab-20261026T183121Z/B/20261026T184245Z-9997b65a`.
The existing `analysis-derived/cpu-phase-samples.json` has partial=false, exit=0;
1141 combat Present rows are the denominator. This is historical sampled
attribution, not a measurement of this backend or current HEAD.

Frame mean17.55055ms, CPU RUN15.73932ms, GPU Busy3.48310ms. Application and CPU
excess over11.11111ms are6.43944ms and4.62821ms respectively. Native material/
scene traversal runs ONCE; recording live eye0 and replaying eye1 does not run
that traversal a second time.

Disjoint render-thread sampled leaf attribution:

- native engine4209/1141 =3.688869ms; identified proxy C++~.787ms and
  system/ambiguous D3D9~.940ms (shared module name; existing symbol classification);
- NVIDIA D3D9 UMD799/1141 =.700263ms; D3D11 UMD165/1141 =.144610ms;
- D3D11 runtime73/1141 =.063979ms;
- kernel5178/1141 =4.538124ms; WOW64 CPU782/1141 =.685364ms.

The kernel/WOW64 categories include syscall marshalling and trace stack-walking.
They are NOT all graphics, avoidable, or assigned to NVIDIA. Driver worker
TID41156 has3355 leaf samples in its hottest nvd3dum PC: that is PARALLEL CPU,
not another2.94ms to add to the render-thread critical path. No driver function
names are invented for unresolved PCs.

Largest identified API-related inclusive subtrees: dynamic VB Lock498/1141
=.436459ms (DdLockLH488/1141=.427695ms inside it), D3D11 Flush437/1141=.382997ms,
DrawIndexedPrimitive319/1141=.279579ms, IssueQuery265/1141=.232252ms,
GetQueryData191/1141=.167397ms. Recorder RecPush2273/1141=1.992112ms includes
VirtualQuery2088/1141=1.829974ms; ReplayStream456/1141=.399649ms. These overlap
other attribution and are NOT summed. Shader constant forwarding is not a
measured multi-millisecond exclusive path. No new profiler or capture was made.

## Compared designs

A. A fully owned state-aware D3D9 frontend could avoid scalar API submissions,
but a local state cache cannot observe all native/private writes or StateBlock
Apply. The previous real Set/Get counterexamples still apply. Correctness would
require owning state blocks, dynamic-buffer versions, all callers and HRESULTs;
a later Get is not an original-input snapshot. Building that replacement here
would risk lighting, multithreading and private COM compatibility before a gain
is established. Recorder safety checks remain unchanged.

B. Select the OS D3D9On12 DDI backend through SYSTEM d3d9's
`Direct3DCreate9On12Ex`. It preserves the Windows D3D9Ex frontend/native COM
objects instead of substituting a foreign API wrapper. The actual changed
boundary is D3D9 DDI -> D3D12 command execution, including resource locking and
submission behavior. This is a small integration with an already implemented
backend, not a new translator or a promise to remove frontend validation.
Microsoft documents the [DDI design](https://github.com/microsoft/D3D9On12).

Also considered dgVoodoo2: its [official readme](https://dgvoodoo2.dege.freeweb.hu/dgVoodoo2/ReadmeGeneral/)
describes D3D9 support and D3D11/12 outputs. That does not establish this game's
private COM calls, legacy shared handles, additional-chain observer or Reset
compatibility. A foreign wrapper has a larger unverified compatibility surface;
no blind chained DLL deployment was attempted.

C. Official [DXVK on Windows](https://github.com/doitsujin/dxvk/wiki/Windows) is
not officially supported. Replacing the frontend also changes private COM
semantics. A viable explicit bridge would have to establish adapter LUID,
Vulkan external-memory Win32 handles/formats, compatible D3D11 imports and
external-semaphore/fence ownership/layout transitions for both eyes. None is
proven by a drop-in d3d9.dll. CPU readback is not an acceptable bridge. That is a
larger separate backend; it was not substituted for the functioning interop.

D. The public scalar Set interface is already the first verified synchronous
input-ownership boundary. No earlier public immutable draw-packet interface is
established. Borrowed pointers, deferred copies or non-atomic setter/getter
recovery are rejected. Native pre-D3D9 packets remain Astra's responsibility.

## Actual implementation

`src/d3d9_backend.h` supplies shared production/fixture admission, factory
selection and requested-device identity verification. `src/d3d9.cpp` selects the
backend for both factory exports. Only the Microsoft SYSTEM runtime is used;
no wrapper/vtable clone, queue worker, hash manipulation or EXE patch is added.
An explicit mode1 request cannot silently fall back to native mode0. Each new
device must QI IDirect3DDevice9On12 before existing device hooks are installed.

`[Render] D3D9Backend=0` is the default and retains the old path. Mode1 requires
the diagnostic protected-Present profile, the native-image write guard,
SubmitMode4 and shared GPU capture. Admission runs during DLL startup BEFORE
legacy hook installation as well as at factory creation. A production build
cannot enable mode1 without its missing protected-Present implementation.
Existing guarded input/head/gesture admission remains mandatory and unchanged.

The key feasibility result is ACTUAL legacy shared-texture interop: 9On12
CreateTexture(shared HANDLE) -> native D3D11 OpenSharedResource on the same LUID
works. Therefore there is no resource Unwrap/Return bridge. Microsoft's
[translation interop contract](https://microsoft.github.io/DirectX-Specs/d3d/TranslationLayerResourceInterop.html)
forbids other translation-layer use while a resource is checked out; using that
bridge amid native workers would require stronger ownership. We do not do it.

Both eyes retain existing producer/consumer event synchronization, GPU copies,
native resource refs, synchronous command ownership and per-eye math. Shared
interop failure rejects the pair and invalidates its metadata. CopyFrame also
rejects mode1, including runtime mode changes: there is NO CPU-readback bridge.
There are no new per-draw logs, allocations, state-elision rules or async driver
calls in the renderer. Existing OpenXR/head/input/gesture code is unchanged.

## Real API verification

`tools/d3d9-stereo-harness/build.ps1 -Backend` extends the existing fixture.
Separate x86 processes test old native and SYSTEM 9On12 through actual production
backend, Present-observer and stereo-math headers. Host: RTX5070, vendor10DE,
LUID00000000:000111D5; 9/11/12 LUID equality checked. OS x86 d3d9on12 version
10.0.26100.9549, loaded from SYSTEM, not a local replacement.

PASS for BOTH backends:

- native additional swapchain -> QI9Ex -> original GetDevice ownership;
- actual production observer:3 entries,3 forwards,3 returns,0 recursion;
- three release/ResetEx/recreate cycles at32x32 ->48x32 ->32x32;
- SM3 VS/PS, native dynamic DISCARD VB, exact state-block PS constant restoration;
- legal concurrent texture allocation/LockRect/Unlock on MULTITHREADED devices
  (named HW_VERTEXPROCESSING | MULTITHREADED flags, not a numeric guess);
- offscreen X8R8G8B8 RT0 -> StretchRect -> shared A8R8G8B8 texture, native
  D3D11 OpenSharedResource/SRV and two independent GPU eye caches;
- actual pixel assertions:121/121 then165/165 then121/121 colored geometry pixels;
  matching material colors and opposite eye disparity centered on the image;
- requested-device identity rejection and320 configuration admission cases;
- fixture main EXE .text byte equality before/after observer and Reset.

The hidden fixture window returns S_PRESENT_OCCLUDED, not D3D_OK. We explicitly
retain that result; the fixture does NOT prove OpenXR admission or game
presentation. Its staging readback is solely pixel-test verification, not a
runtime transfer path. Game-specific private methods and Steam Overlay still
require a separately authorized integration session.

Both x86 renderer builds PASS. Local build SHA256:
- production CF8D84AFB04DCFE2269B7B4196ADDCFE91FC2DB1FAE7B91A06D1E945D753EFC4;
- diagnostic E532D4139C81FD4B6952614777A38A521DBF5C21659B8234DCB2FF300DF7ADEC.

Existing regressions PASS: protected Present six
modes (including overlay recursion, lifetime and EXE-target rejection), native
image guard, head117021, input20032, gestures227, actual recorder239, actual PS
payload576 in both modes, actual float replay432072 records in both modes.
No game assets, EXE dumps, logs, trace files or binaries are committed.

## Performance decision and future integration

KEEP as an OFF experimental backend, NOT as a verified performance fix. The
identified replaceable D3D9 UMD leaf slice is .700263ms/frame. Eliminating that
slice alone would leave mean~16.85ms and CPU~15.04ms, far above native90Hz. That
is an optimistic calculation for the IDENTIFIED slice, not a whole-backend
ceiling: native D3D9 validation remains, D3D12 translation adds work, and related
kernel/driver dependencies are not completely attributed. The wider backend
could help or regress. There is no evidence-backed claim of a6.44ms saving.
RecPush/VirtualQuery and native engine traversal are deliberately not removed.

Worth a future integrated game test: YES, because real GPU sharing, actual
Present forwarding, shaders and Reset work across the replaced execution
boundary. First blocker is game/overlay compatibility with the changed DDI,
followed by same-frame XR output and unique application-frame cost. Even a
positive driver result may leave native/recorder work above the90Hz budget.

No package or launch now. Later use the preserved WORKING protected diagnostic
profile, changing ONLY D3D9Backend to1, keeping BlockNativeImagePatches1,
ImagePreservingPresent1, SubmitMode4, StereoMode1 and ZeroCopy1. Keep existing
working head/input/gesture settings; keep unsafe native hooks and all unverified
performance experiments OFF. Do not copy the repository's legacy INI wholesale.
Preserve current DLL/INI hashes before any separately authorized integration.
A missing system export, backend identity or GPU sharing is a rejection, not an
invitation to enable an unsafe fallback. Rollback: restart with mode0 / restore
the preserved original DLL and INIs. This task did not touch the installation.
