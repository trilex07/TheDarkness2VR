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

## MAIN 2026-10-25 - Approved mono Present trial, baseline restored

Tested exact 7b5ee3d candidate, XR/capture off. Actual swapchain 0F8A0FE0 differs
from implicit 0F781760; caller A94057, thread26060. Last sample 44000 entries =
forwards = returns, recursive0/fault0; all 47 sampled HRESULTs D3D_OK, no logged
exceptions. Per-call HRESULT history is not available in this candidate.
Intro/title observed; gameplay and graceful menu shutdown not validated.
Stopped test PID and restored DLL/both INIs; EXE/local INI hashes unchanged.
No next VR test. Receipt: docs/research/engine-present-mono-20261025.md.
Recorded separate prior-PSVR2 issues VR-001 (P0 right tentacles), VR-002 (yellow
edges), VR-003 (detail loss); prepared one-frame capture plan without execution.

## MAIN 2026-10-25 - Complete HRESULT accounting, next candidate offline

Every diagnostic Present return now contributes to ok/status/error counters;
nonzero HRESULTs log immediately and negative results latch capture rejection.
Added readiness checker with seven tests; x86 ABI/rollback and 15 capture tests
pass. Previous sampled log correctly remains insufficient for full HR proof.
Built isolated candidate; no deployment or new game/VR run. Concrete bounded
test: docs/research/matched-capture-next-candidate.md. P0 remains right tentacles.

## MAIN 2026-10-25 - Approved staged trial: mono PASS, capture incomplete

Exact 4fcef42 mono checker PASS covers 44000 forwarded/returned/OK, zero
status/error/recursion. One approved stereo request frame3822 failed missing
same-eye projection witness: producer is live-only, absent in replay path.
Partial source-eye0 is 1280x855 and already contains yellow edge; includes pause
overlay. 121 draws/eye do not identify right-tentacle geometry. No final images,
COMPLETE or valid Luna analysis. No retry; test exited, DLL/INIs restored with
hashes, EXE/local INI unchanged. Next fix is diagnostic witness provenance only.
Receipt: docs/research/staged-capture-20261025.md. All three issues remain open.

## MAIN 2026-10-25 - Complete capture, shared-transfer race correction

Fixed successful PS-upload witness for both eyes; completed unpaused frame5408
with user in HMD. Source RTs differ but both final eyes contain right-source pixels.
Found internal left StretchRect entering replay before reader fence; moved record
shutdown before internal transfers. Exact tentacle shaders/mesh slices show native
right rest pose above/behind camera; no invented pose correction. Rejected unused
c20 hypothesis before activation. Added separate OFF-by-default73EBA724 previous
projection correction for proven false motion vectors. Production/diagnostic builds,
Present witness,15 capture+7 readiness+3 transfer tests and336 math checks PASS.
Fixed renderer not yet run: another Steam/PSVR2 game active. DLL/INIs restored with
hash verification; no native FOV/EXE disk write, no merge. VR-001 remains OPEN.
Receipt: docs/research/vr001-render-fixes-20261025.md.
