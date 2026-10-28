# Independent equipped pistol models and shots

Implemented on `feature/render-command-worker`; package only, MAIN deploys with rollback.
This extends `ProtectedTrackedHands=1` to the audited pistol class. Legacy EXE
patch installers remain disabled. Native stereo, render resolution and input
trigger routing are unchanged. No FPS or physical headset result is claimed.

## Native boundary and behavior

- Model class `FD6BC0`, 131 methods, slot `F0` -> `AD9240`.
  Copy only its matrix-selection prefix through `AD94A2`'s comparison call.
  Override the candidate stack matrix, then jump to **original `AD94A7`**.
  Both early exits resume at original `AD95C3`. Dirty flags, world matrix,
  bounds, physics and action callbacks run at their original native PCs.
- Fire class `1010AE0`, 33 methods, slot `64` -> `CFBBA0`.
  A private instance-table adapter supplies the same controller matrix to
  **original `CFBBA0` once**, preserving all five arguments and `RET 14h`.
  This is a void thiscall method, not a boolean shortcut to `C47570`.
  The original reference handling and `CFBD60 -> C47570` call remain native.
- Independent OpenXR aim spaces: primary slot uses right controller, secondary
  uses left. Head-reference-relative full rotation and position compose with
  the owner's native camera matrix. Reach is checked in metres; translation
  uses `GameUnitsPerMeter`. Peer tracking loss never gates the active hand.
- Every consumption rechecks reciprocal selected-avatar/camera/inventory/gun,
  currently equipped model, current fire mode and fire receiver ownership.
  Current tentacle acquisition/attachment/alternate target blocks the override,
  preserving native held-enemy/prop animation and actions. No cached receiver
  pointer can authorize a newly equipped gun or a fresh held target.
- Both model/fire tables must be prepared and published before either resolver
  can apply. The gameplay sample recognizes owned model tables after publication.
  Native instances are never retained for cleanup; original destructors remain
  in the copied tables. Code/table storage and the pinned proxy live for the process.

Full EXE identity is pinned to `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
The generator validates the full `AD9240` boundary (`39A` bytes), `CFBBA0`
(`227` bytes), reachable CFG/returns, all 11 prefix calls, two outgoing branches,
helper contracts and complete class tables/header. Runtime validates digests
before immutable private RX/table creation. No EXE/static table/IAT writes,
debug-register operations, or native allocator/cache changes are added.

## Offline validation

- Actual production ownership/resolver/capture source and x86 adapters:
  independent hands; scale, rotation, recenter/base transforms; stale or wrong
  owner/mode/equipped model; newly acquired holds; nullable empty alternate
  state; fallback without candidate writes; original tables unchanged.
- 2,000 fire and 2,000 model direct-vs-adapter comparisons: receiver/arguments,
  stack cleanup, original call counts, return/register/flag behavior,
  x87/SSE/MXCSR and LastError. No game process or headset is involved.
- Prefix copier: overridden and unmodified prefixes, original native tail and
  early exits, full-clone compatibility, malformed plans and RX sealing.
- Existing selected-player, private-table, grip IK/native skin arithmetic,
  left aim and throw regressions remain required, plus both x86 builds.

## Remaining runtime scope

Concrete pistol model/fire ownership only; other weapon classes retain native
behavior. Native debug-camera firing may override its input matrix as before.
Model pivot/muzzle alignment, visible hand/weapon coherence, hits and held-enemy
interactions need one consolidated MAIN headset run. This is not evidence of
90 FPS, corrected side visibility, or permanent right-eye artifact removal.
The user confirmed CULL OFF 5 only in menus and CULL ON during gameplay; this
change does not bypass that admission.
