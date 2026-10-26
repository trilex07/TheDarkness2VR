# Protected replay COM retention candidate

Base: `85eea69`; branch: `codex/protected-replay-performance`.
Status: IMPLEMENTED / OFFLINE VERIFIED. Steam and PSVR2: NOT RUN.
No deployment or game launch. Persistent logging, both eyes, synchronization,
FOV, shaders, resolution and the protected Present path are unchanged.

## One hypothesis and one correction

Existing ETW establishes a render thread with substantial actual execution
(about 11.38 ms/combat frame in the earlier capture), not just a GPU deadline.
It does NOT attribute a measured duration to the COM retention calls below.
Shader creation remains the established large tail-spike producer: 97.865 ms
inside the 130.105 ms B frame. Native loading-phase prewarm is still blocked
by the demonstrated owner/lifetime/selector and loading-gate gaps; this change
neither calls native getters nor claims to repair those stalls.

The verified replay source called AddRef for every recorded resource binding,
and Release for every such call at frame teardown. Rebinding the exact same
interface pointer does not require another retained reference to keep it alive
through replay. Hypothesis: that repeated driver COM bookkeeping is an avoidable
part of render CPU work. Its game-time magnitude remains unmeasured.

`Render.UniqueReplayResourceRefs=1` enables exact-pointer, per-frame retention.
The default is 0, preserving one AddRef/Release per binding. The mode is latched
at RecBeginFrame. The existing record lock serializes ownership. No command,
draw, resource update, graphics synchronization or validation is removed.
Distinct interface pointers remain distinct; no QueryInterface/canonicalization,
shader hash cache, cross-frame COM cache or native-image write is involved.

`ReleaseRecording` releases every owned reference at the existing teardown
point. Reset/lost paths still call it. Nulls are ignored; duplicate pointer
membership ends with the frame, so a new object at the same address in a later
frame acquires a new reference. Bookkeeping allocation precedes AddRef, avoiding
a new leak if vector allocation fails. COM is not called by the ledger destructor
under DLL unload; explicit frame/reset teardown preserves the existing policy.
Existing sparse TrueStereo reporting adds requested/retained reference counts,
so a later authorized A/B can confirm whether real gameplay repeats bindings.

## Offline evidence

- x86 MSVC/O2 production and protected diagnostic DLL builds: PASS.
- x86 regression: legacy accounting; null; exact identity/aliases; native owner
  release before right-eye replay; both eyes; frame/address reuse; clear/reset/
  abort idempotence; per-frame mode change: PASS.
- 10,000 frames per mode, 2,048 bindings/frame and 64 exact pointers/frame:
  COM AddRef/Release pairs 20,480,000 -> 640,000, all resources valid through
  replay and back to owner-only references after teardown. This is a synthetic
  call-volume result, NOT game timing or proof of an FPS gain. Node allocation
  and hashing add CPU cost; if real pointer reuse is low, the opt-in may lose.
- Existing protected-profile regressions: 2/2 PASS.
- Existing rendered-FOV regression: 1,206 ray checks plus output-size bounds PASS.
- `git diff --check`: PASS. No binaries or private traces committed.

Diagnostic artifact retained privately in this worktree:
`dist/d3d9-replay-refs-diagnostic.dll`, SHA-256
`87b86f54870c013c0ae5c99032b4919a8844600256c30394bf2421a79a761c05`.

Rollback: set `UniqueReplayResourceRefs=0` and restart, or revert this commit.
No installed file has changed. First live verification must compare identical
scene/profile with only this key changed, inspect reference-count reduction,
replay wall time and render CPU, and check both eyes/reset/resource lifetime.
No performance improvement or HMD acceptance is claimed before that A/B.
