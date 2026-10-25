# Task branch engineering log

## 2026-10-24 — Offline native camera/FOV follow-up

On `codex/native-camera-fov`, based on research `cb6638f`, verified different
Lua receiver type globals for GetFOV and the base-FOV setter. Identified the
FCOS-to-dot-product candidate filter, perspective-shaped constructors
`00A31FF0` / `00DF3B50`, and eight-corner constructor `00DF53E0` with Ghidra
and static instruction evidence. Active player projection and CPU visibility
producer/consumer are still open; no native hook was implemented or deployed.

Preserved the private Ghidra project outside Git. Added the existing read-only
PE audit and its six passing parser tests to version control, guarded explicit
Ghidra entry assertions with original bytes, and prepared a disabled observation
manifest. The depot sample is rejected by the audit's installed-SHA policy even
though all six instruction sites match. Steam/PSVR2 testing waits for the user
to resolve the antivirus detection and safely restore the installation.

Detailed evidence and limits: [native camera audit](docs/research/native-camera-fov-audit.md).


## MAIN 2026-10-24 - clean offline integration and opt-in matched capture

Approved picks: 744e637, accd9b2, f978db8, 5f59fe1, ddaf4af, in that order.
New dedicated integration worktree; original untracked MODLOG preserved.
Added compile/runtime-off one-frame RT0/final-eye capture with draw banks,
metadata, completeness receipts and checked Luna analysis. No Gemini adapter.
No production INI change or deployment. Ordinary preprocessed production source
matches the integrated baseline. Native camera/CPU visibility ownership remains
open; static worker/dispatcher/bounds evidence and reproducible range hashes
added. Restored installed EXE passes identity/14-site/10-range audit, but AV
resolution and native-patch-free Present observation still block runtime gates.
Details and validation: docs/research/matched-frame-capture.md.


## MAIN 2026-10-24 - Gemini diagnostic toolchain accepted

Integrated ae8de22, 0f9085f, 85be172, 120b9e0 once, preserving previous series.
Acceptance fixes: opaque op=7 viewport becomes UNKNOWN; op=36 clears both shader
constant banks, provenance and pending context so rebind/partial upload cannot
resurrect pre-Apply data. Eight new tests plus original 17 pass, including the
private real trace. Corrected L8=50/D16=80 against local SDK. Existing capture
10/10, PE parser 6/6, Luna self-test PASS. Production src/, INI and build unchanged
versus prior integration; original untracked MODLOG untouched. No Steam work.
Adapter remains diagnostic evidence only; no active-player-camera proof claimed.

## MAIN 2026-10-25 - Actual additional swapchain, observer rejected

Proved 00964C40 CreateAdditionalSwapChain(slot13) -> QI SwapChain9Ex ->
owner+20 -> A94055 Present; owner+1C is the base fallback. One XR-off/stereo-off
Steam trial of a per-instance public-length shadow failed before acquisition:
system D3D9 calls private device slot156, beyond Device9Ex's 134 SDK methods.
Removed the unsafe runtime prototype, restored installed DLL/INIs with matching
hashes, and stopped further live testing. Added reproducible EXE/module byte
audit and six fail-closed regression tests. Actual Present callbacks remain zero.
Details: docs/research/actual-swapchain-observer-20261025.md. No FOV work or merge.

## MAIN 2026-10-25 - Isolated engine Present candidate, approval pending

Prepared OFF-by-default EnginePresentCallsite diagnostic exception using the
historical A94052 CALL dispatch, exact SHA/PE/site checks, first-CreateDevice-only
installation, recursion rejection and current capture witness/return gate.
No COM shadow. Offline x86 instruction/ABI/rollback tests, frame suite, 15 capture
and 13 audit tests pass; production preprocessing unchanged. No deployment or
game launch. Explicit user approval required for the in-memory EXE change.
Details: docs/research/diagnostic-engine-present-candidate.md.
