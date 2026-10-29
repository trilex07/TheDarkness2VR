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

## Shared hand/weapon world scale correction

Protected grip IK previously composed metre translations directly into native
world coordinates while pistol aim used `GameUnitsPerMeter`. Both now use
`BuildScaledWeaponMatrix`: validate physical reach first, scale translation
only, retain rotation and the native camera origin, and reject invalid scale
without writing output. Scale 1 preserves the previous arithmetic exactly.
Legacy IK passes scale 1. Actual protected-hand tests now cover scale 0.5 for
both wrists, invalid-scale no-write behavior, and unchanged tentacle bones.
58,246 hand checks and 74,467 weapon checks pass, including the existing exact
native skin arithmetic fixture. This corrects a nondefault-scale pose mismatch;
it is not an FPS improvement or a physical headset acceptance result.

## Held-character sampler consistency, 2026-10-29

CaptureNativeGameplay previously classified Busy from +558/+580/+590 but
classified alternateHeldTarget from +584 as well. A character held only in
+584 therefore published Empty plus alternateHeldTarget. The real gesture
recognizer could reacquire that target or discard hold intent after its empty
grace period. The new actual-sampler-to-recognizer regression fails the old
implementation at protected_weapon_test.cpp line94 (expected Busy).

An alternate held target now also publishes Busy. Nullable +584 handling,
ownership checks and ordinary-prop aim/velocity exclusions remain intact.
The x86 actual-source fixture passes 74711 checks, including four separately
populated native target fields, sustained grip, one release stroke and return
to Empty. Existing throw218 and left-aim461 checks pass. Both DLL flavors build.
These CPU fixtures do not validate native enemy lifecycle or headset behavior.

## Held-target headset feedback corrections, 2026-10-29

The user reports that a demon hold disables tracked human hands/pistol, the
one-handed pistol sits above the hand, and head-attached tentacles separate from
their held target. Three independent source contracts account for these effects:
the human packer and pistol resolvers explicitly rejected Busy/alternate holds;
human weapon sockets used grip space while the pistol used aim space; and a
mesh-scoped rendering correction cancelled the head transform only on tentacles.

The protected packer now permits holds while preserving root and demon bones
0..1/46..129; legacy hand hooks retain their Busy refusal. The equipped gun is
revalidated through its own inventory/model/fire ownership chain on each use.
An equipped hand uses that gun's aim pose, preserving the existing native socket
calibration. A hold cannot authorize foreign, replaced or dropped guns. The
tentacle correction requires fresh Empty native feedback, so held target and
tentacle remain in their unmodified native world space during acquisition/holds.

Actual-source x86 fixtures pass 58,828 hand checks (including native B1E500 skin
arithmetic), 74,795 weapon/sampler checks, 19,069 draw-scope checks and 774 gesture
checks. Repeated held-hand packing has no accumulated delta and leaves demon
bones byte-identical. Both DLL flavors build. The leg reload and control/overlay
overrides are described in profiles/controller-playtest-fixes. Sleeve cutting
remains unimplemented: the two captured human-arm materials do not yet identify
sleeve triangles. These are offline corrections awaiting the user's HMD test;
no physical grip, native attachment continuity or performance improvement claimed.

## Located grip velocity, 2026-10-29

LocateTrackedHands previously ignored runtime velocity and differentiated predicted
grip positions over game-frame intervals. That is an interval average, not the
instantaneous motion estimate at release. It now chains XrSpaceVelocity onto the
existing grip xrLocateSpace call and uses linearVelocity only with LINEAR_VALID,
tracked grip and finite output. There are still two locate calls per active hand
(grip and aim), no extra sampling call, allocation or per-frame diagnostics.
The [OpenXR contract](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrSpaceVelocity.html)
defines velocity in the locate's base space, in metres per second. Motion's
first-sample/recovery, 2–100 ms interval and 0.5 m jump gates apply to both sources.
Unavailable or nonfinite velocity retains the old position-difference fallback.
Nonfinite pose/time is rejected. Powered-stroke selection, native throw ownership,
native magnitude/spin, the character branch exclusion and aim assist are unchanged.

The actual LocateTrackedHands fixture fails commit 2e9b887 at line81: a valid
runtime velocity is lost when adjacent predicted positions coincide. The correction
passes 2,756 checks across both hands, action/location failures, missing/invalid
velocity, recovery, gaps, jumps and numeric faults. A known accelerating trajectory
checks the instantaneous derivative at 40/45/72/90/120 Hz; actual located output
also drives the existing Tentacles recognizer through a single powered release at
those rates. These are deterministic CPU/OpenXR-boundary fixtures, not measured
headset motion or game performance. Existing hand isolation752, gesture774,
protected native throw218 and 2,000 native-vs-bridge ABI comparisons pass. Both
production and diagnostic x86 DLLs build. The new build is packaged separately;
the installed 2e9b887 candidate is retained for the user's consolidated test.

## Original hands without sleeves, 2026-10-29

The stock H.Misc resource `/D2/Characters/Camera/FPJackieArms_skel.fbx`
explicitly assigns HandRevised and SleeveRevised materials and separate index
ranges for five LODs. Its B.Misc payload contains byte-identical copies of the
114400-byte VB and85818-byte IB retained in the matched stock frame. This
identifies the original asset without guessing shader hashes or bone weights.
The actual private geometry preview confirms that the second part is sleeves.

`tools/hands_only_asset.py` pins the extracted header/body and compressed cache
preimages. It degenerates only the3500 sleeve triangles across all five LODs,
retaining each triangle's first index inside the original draw's vertex range.
Every hand index, vertex, skin weight, skeleton byte and texture stays original.
No model data is distributed in Git. Only the normal FPJackieArms model is
changed; asylum/crucifixion and other avatars are not silently replaced.

Installation appends the206112-byte modified cooked resource as an uncompressed
entry (zsize=size) and atomically redirects only its existing B.Misc TOC record.
The entire original cache prefix is untouched. A recovery receipt and original
TOC are written before mutation. Rollback verifies before/after file hashes,
refuses foreign changes, restores the TOC and removes only the appended tail.
A verified partial append is also recoverable. No EXE, shader, runtime settings,
SteamVR setting or DLL change is required by this asset correction.

Six generated-data tests cover all LOD triangles, hand/vertex preservation,
foreign preimages, repeated application refusal, exact install/extract/rollback,
failed TOC replacement, partial appends and refusal after external mutation.
Actual stock preparation and geometry preview pass. Native rendering and wrist
appearance still require the user's headset test; offline removal is not proof
of in-game acceptance.

Private preparation:
`python tools/hands_only_asset.py prepare --header <extracted H asset> --body <extracted B asset> --output <new private package>`.
Closed-game install:
`python tools/hands_only_asset.py install --cache-dir <game/Cache.Windows> --package <private package> --backup <new backup directory>`.
Recovery:
`python tools/hands_only_asset.py rollback --receipt <backup/receipt.json>`.
