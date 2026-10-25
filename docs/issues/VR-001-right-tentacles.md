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
