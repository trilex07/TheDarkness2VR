# VR-001 — Missing right Darkness tentacles

Priority: **P0**, assigned by user. Status: OPEN, cause UNKNOWN.
Source: user-operated PSVR2 feedback on 2026-10-25 from an earlier build/session,
not diagnostic candidate 7b5ee3d. Exact previous DLL/config not established.

Symptom: user cannot see the right tentacles. Do not assume missing geometry,
wrong eye, culling, skinning or postprocessing without evidence.

Next evidence: one approved matched gameplay frame, both D3D9 RT0 sources and
both final D3D11 eyes, plus matching draw banks/projection/viewport metadata.
First determine the earliest stage at which visible right-tentacle pixels are
missing. If already absent in both sources, inspect the corresponding draw
submission/geometry and visibility path; draw presence alone does not establish
pixel visibility. Fix only the demonstrated failing stage.

Acceptance: matched-frame evidence of correct geometry in both eyes, then a
separately requested user PSVR2 check. No fix or acceptance claimed yet.

2026-10-25 controlled capture: incomplete, right-eye projection witness missing;
121 draws per eye but no proven tentacle mesh attribution. Left-eye RT0 contains
a pause overlay, so it cannot settle right-tentacle absence. No final eye images.
See ../research/staged-capture-20261025.md. Remains P0 OPEN; no culling diagnosis.

2026-10-25 follow-up: complete unpaused frame5408 now contains both sources,
both final eyes and identifiable right-palette/mesh slices. Right geometry is
already projected above the top clip plane/behind the camera by the native
rest-pose palette. Mod leaves these c45 uploads unchanged. The requested right
attack check was not completed (user closed game); stock same-save control was
blocked by an active Singularity VR session. No arbitrary reposing was applied.
Independent stereo-transfer defect WAS fixed in src: stop recording before the
mod's shared-texture copy, preventing its replay from racing the left-eye read.
This is not a native right-tentacle pose fix. Runtime confirmation still pending.
Evidence and rejected c20 experiment: ../research/vr001-render-fixes-20261025.md.
