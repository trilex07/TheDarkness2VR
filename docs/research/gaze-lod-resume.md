# Gaze LOD investigation resume, 2026-11-01

User redirected active work from wheel artwork to native gaze-driven CPU LOD.
Latest prior branch: codex/gaze-lod-observation in
C:/Users/Trilex/vr-mods/Darkness2VR-engine-frame-cap (read-only inspected).
Prior agent stopped on usage limit. No runtime optimization was established.

Existing offline Ghidra evidence outside repository:
C:/Users/Trilex/darkness2-native-re-settings/gaze-lod-selector.txt and
gaze-lod-settings.txt. Their executable SHA256 matches stock
D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.

## Concrete selector evidence

AE30C0 traverses object pointers. ESI is current object, EDI view/camera.
It derives point-to-AABB distance using view+140 and object bounds+130/+140,
multiplies by a view-dependent factor, stores object+2D4, then multiplies by
global 110E074 at AE319E. Resource object+1C4 virtual+15C returns a table
of 8-byte entries. Successive float thresholds select an integer index stored
in object+2B0 (AE31FA and subsequent writes).

AE326E applies object+2E0 minus global110E048 to that index. Resource virtual
+124 gives the count; AE3290..AE32A5 clamp the result to [0,count-1] and write
object+2B0. This is strong static evidence of a bounded per-object LOD index,
distinct from BB79A0/C5D850 dependency priority. Actual mesh consumption and
CPU benefit remain unproven.

8513A0 dispatches selectors. Global110DE54 and view+618 gate LOD; global
110DF40 selects6281F0; otherwise mode110DFF0 (fallback view+61C) selects
AE30C0 versus6BF3D0. Do not assume AE30C0 covers every scene/view.

## Next evidence required

1. Follow reads of object+2B0 into resource/mesh submission and establish type.
2. Trace alternative selectors6BF3D0,6281F0 and default5F6160.
3. Identify decoration-only ownership, selected gameplay view, stereo sharing,
   and safe observation point before any per-object quality changes.
4. Observe native selected index, resource level count, gaze angular distance
   and CPU submission cost. No global LODScale mutation as a gaze substitute.

No game files, installation, process memory or quality settings changed.

## Consumer and alternative-selector pass

Private export gaze-lod-mesh-consumers.txt generated using existing RootCause
Ghidra project read-only, stock executable hash above. No project edits.

- 6281F0 sets object+2B0 to resource virtual+124 count minus one. This
  establishes the force-last-level path; 5F6160 resets index to zero.
- 6BF3D0 uses distance, view/FOV and viewport-dependent terms with resource
  virtual+158; applies object+2E0 and clamps to count. LODScale in AE30C0
  therefore cannot be assumed to affect this alternative selector.
- BBAA80 belongs to a preparation routine that passes object+2B0 together
  with a resource sub-entry index to resource virtual+160, gating subsequent
  work. Object resource remains at+1C4. This links the selected index to a
  per-resource eligibility query, but not yet to a proven triangle budget.
- 57327D and AD4F8D pass the index to resource virtual+128 and convert the
  returned value and index to floats. Their decompilations have warnings;
  do not infer mesh submission solely from those calls.
- 68D069 is an unrelated destructor field collision. Searching displacement
  2B0 alone produces hundreds of false candidates (including virtual slots).

Next: resolve actual resource vtable implementations of+124/+128/+158/+160
for observed renderer resource types, then trace the geometry submission
consumer. Do not patch generic offset matches.

## Implemented observation policy (not wired to runtime)

src/gaze_lod_observation.h classifies a verified object bounding sphere against
fresh gaze. Full angular footprint protects large objects overlapping the
35-degree central cone. Unknown owner, invalid bounds/index, future/stale gaze
and invalid direction reject the observation. Candidate flag requires explicit
decoration classification and an available lower-detail level. It does not
change native memory or infer decoration from an address.

MSVC x86 /W4 /WX test/gaze_lod_observation_test.cpp: 13 checks, zero failures.
This is offline policy validation, not a hooked observer or measured CPU saving.
Native owner/resource admission and runtime observation integration remain open.

## Runtime connection preflight

Installed DLL independently hashed to
7DB3D6475C8E78E25A793226559504A9B848EC8B7047DD0588E8571398D5FE38.
No DarknessII process returned by preflight process lookup. This does not grant
permission to overwrite the newer v11/frame-cap integration with this checkout.

Read-only Ghidra export gaze-lod-view-entry.txt proves two direct selector
calls:58290A inside5828B0 and B2A147 inside B2A0E0. First caller receives
view-list argument on stack (ESI), renderer in ECX/EDI; invokes8513A0 with
that list. The argument is NOT the scene reference passed to the existing
985xxx selected-avatar observer. Reinterpreting that existing argument as the
LOD list is unsupported and would risk invalid memory traversal.

Required before deployment: connect a verified list lifetime at either real
caller (or prove its route from the owned renderer), preserve protected-native
EXE invariance, and integrate against the archived installed source. Existing
hand-resource private hook only owns avatar hands, not scene decorations.
No runtime hook or installation was performed in this preflight.

## Latest-source configuration diagnostic

Extracted exact installed source.zip into build/gaze-lod-diagnostic; archive
omits third_party, supplied from current checkout. Added standalone tracked
src/gaze_lod_diagnostic.inl after eye_gaze.inl and its call alongside the
existing XR-running ApplyEngineFrameCap call in that isolated source copy.
Render/GazeLodDiagnostic=1 enables a five-second configuration/gaze witness.
Default OFF. Native enable/scale instruction bytes checked against actual EXE.
Logs explicitly say config-only and object observation NOT connected.
No settings or native image writes. This identifies effective mode and fresh
gaze availability; it cannot measure per-object LOD or CPU savings yet.

## Installed diagnostic (2026-11-01 01:50 Europe/Warsaw)

Installed protected-native candidate SHA256
148538F92CB37B2AF2D166282972B29FF1C23E46E7D70BDB1787D700719889CF
from exact 7DB3D647 installed-source archive. The game was closed. Two active
profiles have Render/GazeLodDiagnostic=1; existing EngineMaxFrameRate=120 is
preserved. See HANDOFF-gaze-lod-diagnostic.md for complete package/rollback.
No headset result or object-level observation yet.

## Later MAIN read-only owner result (2026-11-01)

`HANDOFF-gaze-lod-optimization-20261101.md` verified one active stock `00FF8818` owner in a live 3D scene, with non-null `+274` renderer and scene-list references. The hardware execution observer at `008513A0` refused occupied foreign DR state; no native quality setting or object LOD was changed. A CPU.Verbose ETW profile was refused by Windows policy. The stable gaze-dot/R2 DLL was restored with caches retained. This proves a candidate owner path, not safe decoration classification, mesh cost or CPU improvement.

## Read-only view-list path, 2026-11-01

The stock `00776DC0` renderer path invoked from owner virtual `+0x94` builds the argument for `00AEA1A0` at `0077751D`. It traverses a linked list with sentinel at renderer `+0x124`; each node's inline view subobject starts at `+0x10`, and that address is wrapped on the stack. `00AEA455` passes the wrapper to `005828B0`; `0058290A` then calls selector dispatcher `008513A0`. The wrapper is transient on the native stack, and some branches choose an alternate node through renderer `+0x14C`; the persistent list is only a partial observation path. This does not expose the selector's object list or establish that a view node is decorative.

`tools/lod_owner_scan.py` now checks the live process EXE SHA256 before reading and, for an admitted `00FF8818` owner / `00FF89E8` renderer, follows at most 64 view nodes from that sentinel. It reports the inline view address, mode `+0x61C`, LOD gate `+0x618` and kind `+0x178`; cycles, changed head and unreadable pointers stop observation. No native writes, debug registers or DLL changes. Python AST passed and a deliberate non-game process was refused by the SHA gate.

## Live 3D view and selector receipt

With the user-controlled game running (PID 32160), the corrected read-only scan found exactly one admitted owner at `0E9AFC00`, renderer `0E9E3AA0`, and a complete two-node view list. Nodes `2701A980` and `2701AFC0` contain inline views at `+0x10`; both reported kind `0`, view LOD gate `1`, and mode `8`. Process-local addresses must not be reused in later runs. The stock executable SHA gate matched `D46BB6...`.

Read-only globals were LOD enable `1`, force-last `0`, signed mode override `-1`. Stock dispatcher `00851443..00851462` uses view mode when the override is negative; mode greater than zero selects `006BF3D0`. Thus `006BF3D0` is the predicted selector **if these views reach the dispatcher**. There is no live call witness because foreign DR state prevented the execution observer. `006BF3D0` writes the same per-object `+0x2B0` index but does not read global `0x0110E074` (`Graphics.LODScale`); it uses view `+0x61C/+0x620/+0x17C/+0x34C`, object bounds and resource virtual `+0x124/+0x158`. The whole-scene EngineLODScale trial cannot establish that this active view path changed geometry or saved CPU.

Renderer `+0x5B4` still points to a resource whose `+0x200` field is nonzero. That field is a **reference**, not an established list count. The transient wrapper/object list passed to `006BF3D0` and per-resource mesh class are not yet identified. No safe per-object mutation or CPU saving is claimed.
