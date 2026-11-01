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
