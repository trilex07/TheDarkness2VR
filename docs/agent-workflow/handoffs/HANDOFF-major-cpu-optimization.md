# Major CPU optimization continuation

**Task ID**: major-cpu-optimization
**Date**: 2026-10-31
**Role**: MAIN
**Branch**: codex/main-single-owner-d3d9-validation
**Status**: ACTIVE; stable native 90 FPS not achieved
**Baseline**: installed f24ced92 / DLL 34641A8981444EA05723E6F5F3D669099F6FECD3ABC1142ED1C686C6E42EC232

## Reviewed accepted work
2f24b31e is ALREADY an ancestor of MAIN 75432ae3, and its exact one-lease
Clear implementation is present. Do not cherry-pick it again. Actual callback,
nested-lock and lifetime tests rerun on Windows. ConsumedRecordCopy=1 and
all deployed files remain unchanged. Accepted recorder RPM exposure reduction
(~0.92 ms) is retained; no extra saving is claimed by this continuation.

Latest complete retained ETW is 20261030T221816Z-61b77f88, TID7828,
1528 combat intervals, partial=false. Reaggregation: native EXE leaves
6485 / 1528 = 4.244 ms/frame; ambiguous d3d9.dll module leaves 5252 / 1528
= 3.437 ms/frame. That module name combines system D3D9 and proxy. These
are sample exposures, include necessary work, and predate mode4. They must
not be presented as current mode4 costs or all recoverable milliseconds.
Latest light mode4 pair is documented in HANDOFF-single-owner-d3d9.md:
13.73 ms app mean, no stacks, unresolved 42.656 ms producer-scope outlier.
No fresh capture, RPM/VB-copy/shader-cache reinvestigation or Steam change.

## New C++ ownership prerequisite
src/stateblock_lifetime.h introduces per-instance six-method StateBlock9
shadow dispatch on native object DATA, preserving pointer/IUnknown identity,
native AddRef/Release and exact HRESULT. No EXE instructions, OS dispatcher,
shared driver vtable or executable page is changed. Original native methods
run with no registry lock. Bounded 4096-entry registry, allocation/CAS failure
returns false, thunk/observer modules are pinned before publishing dispatch,
last native Release retires the record, outstanding version
observations keep shadow metadata alive. Successful unknown/distinct QI aliases
notify the observer and must permanently deny native cache admission.
Capture invalidates version before/after native work; Apply preserves content
version. Reset never globally clears records: app blocks may survive ResetEx.
Versions are NOT immutable snapshots or locks; replay must hold native COM
refs and revalidate after execution, with foreign/reentrant mutation denied.

This code is deliberately NOT included in d3d9.cpp or connected to admission.
No configuration is changed, no OpaqueStateBlock/UncontrolledMutation denial
is cleared, and this prerequisite currently saves ZERO runtime milliseconds.
Its purpose is to make the existing measured ~1.8 ms float/binding submission
exposure eligible for a defensible future review, not to assert all exposure
is redundant or promise a multi-ms gain from a registry.

## New actual-driver regression
build_stateblock_lifetime.ps1: MSVC x86 /O2 /W4 /WX, actual system D3D9Ex HAL.
128 native StateBlock lifetimes per mode (HAL/Ex flags 0x20 and actual mode4
flags 0x50), unchanged native pointer and QI identity,
GetDevice/refcount/results, Capture generation invalidation, correct render
state after Apply, four ResetEx survivals, and a two-second callback worker
that must acquire the registry while the observer is active. PASS.
Modelled faults also verify E_FAIL Capture, S_FALSE Apply, successful unknown
QI escape and 4096-object capacity refusal without pointer/result substitution.
Eight relevant x86 runners and both ordinary/DiagnosticCapture builds PASS.
No game/OpenXR execution and no pixel/FPS proof inferred from this state test.

## Remaining implementation gate
Do not simply enable filtering after installing an Apply hook. Integration
still needs proof of all native Apply/Capture/alias edges, correct active shader
metadata, immutable recorded Apply semantics or rejection when Capture changes
content, driver callback safety, registry lifetime and Reset under actual-source
regressions. Keep old permanent denials until this complete contract is wired.
Do not revive the legacy epilogue-scanned StateBlockHook implementation.
The review of shared-capture reuse is separate on side/cpu-hotpath-opt f923c9c5;
it remains OFF/rejected for automatic enablement pending combined-path proof.

## Architectural candidate rejected before implementation
Naively returning to native game work before finishing the right transfer
would defer SubmitXR. Current VrFrameBody calls SubmitXR, then BeginTrackedFrame;
the latter refuses to run while the previous tracked frame is open. Skipping
that barrier without a two-frame pose/display-time ownership protocol would
render the next native frame using the previous frame's latched head matrices
and hands. Do not introduce this head-lag regression to hide a ~1.4 ms join.
A valid overlapped publication architecture needs separate immutable XR frame
contexts and proven Wait/Begin/End ordering, not a moved wait or stale image.

Private evidence/build/test logs:
%LOCALAPPDATA%/Darkness2VR/packages/main-stateblock-ownership-20261031.
Next: finish the StateBlock admission contract only if exact source/driver
regressions prove it; quantify actual duplicates before claiming gains.
The native-engine closed resource cycle remains excluded. Full 90 Hz goal active.
