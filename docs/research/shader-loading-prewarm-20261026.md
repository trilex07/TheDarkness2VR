# Shader loading prewarm: native admission is blocked

SIDE branch: `codex/shader-loading-prewarm`, based on `e2cb9c8`.
Decision: **BLOCKED, no runtime prewarm candidate admitted**. No Steam launch,
deployment, renderer/config change, asynchronous D3D9 work or shader COM reuse.
The persistent-log correction is retained. This is a specific safety blocker,
not a game fix or a claim of reduced stutter.

## Verified path and new evidence

The existing ETL attributes 97.865 ms of the 130.105 ms worst B frame to
shader-creation waits. It does not identify which future native variants must
be prepared during loading. The following static results use the existing
Ghidra project and EXE SHA-256
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.

- Native PS getter `00493180` uses an ECX owner and two stack feature words
  (`RET 8`), device at owner `+0x0C`, bytecode array at `+0x30..+0x34`, and
  successful shader-object slots at `+0x60`. It calls CreatePixelShader at
  `00493273` (D3D9 slot106), only for an empty object slot and non-null bytecode.
- Native VS getter `00846E70` has the equivalent contract, object slots at
  `+0x50`, and CreateVertexShader at `00846F63` (slot91).
- `00CF83D0` extracts selected bits from the two feature words using native
  mask/count data. Its result indexes a second native dispatch table, whose
  value selects the physical shader slot. Getter arguments are **not physical
  variant indices**. Calling `(variantIndex, 0)` in a loop cannot enumerate the
  variants correctly without the owner-specific masks and dispatch table.
- Entry xrefs connect both getters to `00B4D2B0`: VS calls `00B4D435` and
  `00B4D4C4`; PS calls `00B4D448` and `00B4D56E`. The currently analyzed entry
  reference to `00B4D2B0` is a data dispatch-table entry at `0101B758`; this
  does not establish the concrete owner type or lifetime of that dispatcher.

`00B4D2B0` is a shader-binding/state-update operation, **not a proven loading
callback or a side-effect-free preload API**:

- Native resource descriptors at binder `+0x2C`, `+0x60`, and `+0x64` determine
  the path. In the analyzed branches, state2/3 invokes `00B61190`; state4
  supplies the getter receiver through descriptor `+0x30`, then `[0]`.
  `00B61190` is not established as a complete loading-phase callback.
- Binder feature words `+0x40/+0x44` feed VS; `+0x48/+0x4C` feed PS. This
  receiver is distinct from each getter's owner; the offsets are not portable
  between these object types.
- It updates native last-material/feature state, shader-related arrays and
  cached bindings. At `00B4D7E3` it calls D3D9 SetVertexShader (slot92);
  at `00B4D807`, SetPixelShader (slot107), using binder device `+0x1A4`.
  Invoking this function as an invented warmup callback would mutate the live
  binding state. Saving only D3D9 state would not restore its native bookkeeping.

There is a concrete failure consequence for calling the getters too early:
on failed CreateShader, PS `004932E8..004932F2` and VS
`00846FD8..00846FE2` clear the native bytecode slot before storing a null
shader result. An incorrectly admitted prewarm can therefore remove that
owner's subsequent creation input. No failure suppression or retry-behavior
change is proposed.

## Why the existing proxy cannot safely admit prewarm

`src/stereo_replay.inl` intercepts CreateShader with only the D3D9 device,
bytecode pointer and output pointer. These arguments do not identify the
native owner/selector mapping or prove any future owner's residency. The
native per-slot compare/create/store sequence in these getters has no local
atomic admission or lock; external serialization/lifetime ownership is
unresolved. Additional concurrent calls are not justified by a successful
D3D9 device creation or by the mod's metadata lock.

The existing `g_vsInfo`, `g_psInverse`, `g_psViewExtents` and pointer/hash maps
describe already-created shaders and stereo reflection. They are not a native
owner registry, do not retain the getter receiver, and cannot enumerate cold
variants. Recorded RecOp commands include shader binding/constants, not a
native owner allocation/destruction or future-variant registration stream.
COM AddRef on an already-created shader would not retain its native owner or
populate another native slot. A hash-only COM cache would not solve this gap.

The only relevant current world-phase flag is `g_in3DGameplayMode`, updated
from `d2vr_input::WorldActivity` in SubmitXR. Its inputs are recent world-camera
draw observations and time. A title/menu and a loading screen can both have no
world draws; a paused rendered scene can still have world draws. The source
explicitly marks native gameplay/menu state unverified. Negating this flag,
using small draw counts, or waiting for Present is not a verified loading gate.

`g_renderThreadId` is assigned in VrFrame. That supplies an observed render
caller, not proof of exclusive access to every native shader cache. The VS
hook already protects shared metadata against off-thread creation, but that
metadata lock is not the engine's owner-lifetime or loading lock. Actual
cross-thread native cache ownership remains UNKNOWN.

## Exact gate to a real candidate

A bounded loading-only candidate needs all of the following connected to the
same native resource objects before it can call the existing getters:

1. A verified loading entry/exit signal and correct dispatch thread/device.
2. A live native shader-owner set, with retention and unload/reset invalidation
   rules, not saved raw addresses from one CreateShader call.
3. Valid owner-specific feature/mask/dispatch combinations for required cold
   slots, and proven resource/device readiness before native failure paths.
4. Exclusive native cache access, a bounded work budget, and unchanged getter
   success/failure semantics and ownership.

The newly traced binding path narrows the next static query to the dispatch
owner at `0101B758` and resource transition `00B61190`, followed by their real
loading owner/lifetime. It does not authorize a vtable hook at that address.
No new broad profile, toolchain survey or Steam attempt is needed to establish
the current blocker. MAIN separately owns crash stability.

## Offline checks and reproducibility

Only the existing `CameraEvidence.java` was run, read-only/noanalysis, against
`RootCause.gpr`. Entry queries were `00493180`, `00846E70`, `00CF83D0`, followed
by `00B4D2B0` and `00D37B90`; both exports contain `EVIDENCE COMPLETE` and the
pinned EXE identity. Instruction-level ECX/stack and slot displacements were
checked against decompiler output; the decompiler alone omits implicit ECX
receivers at getter calls and was not used to infer those receivers.

Private export SHA-256:
`shader-binding-side.txt` =
`2226873c4a5207d67240d4f9de35c4507a5e6bd473e4b2fe4f61d5a4c11f89b6`.
Exports remain outside Git; no game code dump, bytecode or private log is
published. `git diff e2cb9c8 -- src Darkness2VR.ini Darkness2VR-diagnostic.ini`
is empty. There is no runtime implementation to regression-test, and no
synthetic PASS is substituted for missing ownership or loading evidence.
Steam/PSVR2 prewarm verification: **NOT RUN**. Fewer stutters: **NOT VERIFIED**.
