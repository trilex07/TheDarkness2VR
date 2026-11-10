# Review packet A: defer one swallowed new grip edge

Date: 2026-11-10. SIDE darkness2. Branch: codex/iron-bar-grip-retry-20261110.
Worktree: C:/Users/Trilex/Darkness2VR/.iron-bar-retry-20261110.
Clean base: accepted source2567d04072e9a38c66faec017e3b2779471935e5 / installed39D.
No BFE/088 source, other root edits, runtime, install or profile edits included.
This is an opt-in hypothesis candidate, not proof of the player's event cause.

## Exact semantics

`[Input] DeferredGripRetry=0` by default (missing key is OFF). A future reviewed
trial could set1 in the actual active game INI; no current INI was edited.
Legacy first-press/continuous-hold behavior and native eligibility stay intact.

1. The existing grab pulse records `grabAt`. Existing 650ms acquisition grace
   remains unchanged. A continuously held original grip cannot create a retry.
2. Only a separate release/new squeeze edge which the original acquisition
   intent would swallow **inside that grace**, with fresh verified native Empty,
   can set `deferredGrab`. It does not emit an early LB pulse.
3. At/after the same original650ms boundary, while still held, one fresh Empty
   observation can emit one ordinary120ms LB pulse. It resets stroke history,
   clears the pending edge and begins normal acquisition intent for that pulse.
   That same hold cannot enqueue another retry or collect a pre-retry throw.
4. Release, Busy, Unknown, failed/stale sample, invalid grip OR aim, invalid clock,
   owner change, pause/context loss and disabling the feature cancel the pending
   edge. Focus, native menu, wheel, heart and support ownership also cancel it.
   Restoring the old context while still held does not resurrect the edge.
5. No new native query or eligibility bypass: `ApplyProtectedLeftThrow` keeps
   its owned camera/fire/tracked-head/trigger/context checks and calls the same
   `CopyNativeGameplay` (<150ms) before passing feedback to the recognizer.

Support isolation is explicit: `SyncOpenXRGamepad` passes the complete local
`swallowLeftGrip` latch into `GripRetryContext.support`, **not** the narrower
`g_twoHandSupportIntent` (which can fall when supportContext drops). The latch
persists until release/tracking loss. Thus loss of current weapon-support context
cannot authorize a deferred edge from that same support grip. The existing heart
ownership branch still resets both the protected recognizer and native token.
This patch gates deferred retry only; it does not rewrite original support,
initial LB, heart, reload, native buttons or turning policy.

## Actual-body CPU proof

Command (x86 MSVC, external temporary fixture directory):

```powershell
$env:TEMP='C:/Users/Trilex/.codex/tmp/darkness-grip-retry-20261110'
./test/build_protected_left_throw.ps1
./tools/frame-boundary-simulator/build-protected-gestures.ps1
./test/build_heart_pull.ps1
./build.ps1 -DiagnosticCapture # build only; never -Install
```

Private evidence: `captures/grip-retry/`.
- `red.log`: with ownership/config wiring but baseline state machine, actual
  wrapper fails `Step(680,1,0,0)` (the required deferred LB is absent).
- `red-release.log`: removing the pending-entry stroke guard fails a moving
  release at290ms; demonstrates why cancelling an edge must not become a throw.
- `green-freshness.log`: **1040 actual-body checks PASS**, plus21 pinned native
  instruction contracts. Includes defaultOFF, original hold, exactly one retry,
  no pre-grace pulse, release/moving release, Busy/Unknown, owner change, native
  context, feature disable, clock gap, each input owner and queued throw.
- Freshness uses the **actual CopyNativeGameplay body** (only function renamed
  in fixture), not merely a mocked success flag: a100ms sample passes;151ms fails
  while input polls remain timely; a fresh later sample cannot revive the edge.
- Separate grip/aim invalid cases use the exact production producer conjunction
  `hand.valid=hand.gripValid && hand.aimValid`; builder verifies that source
  contract and the full support-latch receipt before extracting the wrapper.
- `gestures.log`: **913 checks PASS**; `heart.log`: **62 checks PASS**.
- `product-build-final.log`: protected-native x86 DLL built offline. Product
  artifact is preliminary/review-only, not installed or finally frozen.

The source-to-wrapper wiring is statically reviewed, not live XR-proven. No
runtime scene, native grab success, player-event cause or headset acceptance is
claimed. Existing fixture C4459 shadow warnings remain. Initial sparse checkout
build missed the pinned weapon icon header; checking out only its tracked source
dependency fixed the build. No arbitrary root assets were copied or changed.

## Review and rollback

Review `git diff 2567d040 -- src/d3d9.cpp src/protected_left_throw.inl src/vr_input.h`.
Manifest and source patch are private under `captures/grip-retry/`; they identify
the exact review commit/source hashes and local DLL. No frozen A76/BFE modified.
Current installed39D remains untouched. Any later serialized trial must take
fresh current rollback/settings/saves, change only the candidate DLL plus this
one explicitly enabled key, then restore fresh39D/INI/saves/config with hashes
and XR/Steam verification. DefaultOFF or removal of the key preserves baseline
input behavior after restart; original39D remains the artifact rollback.
