# VR-002 — Yellow artifacts at field-of-view edges

Status: OPEN, cause UNKNOWN. Work after P0 VR-001.
Source: user's earlier PSVR2 session, not mono diagnostic candidate 7b5ee3d.

Symptom: yellow peripheral areas persist. FOV/frustum/clip/occlusion, lighting
and postprocessing are candidate explanations, not established causes.

Next evidence: locate the same region in source-eye0/1 and final-eye0/1 of an
approved matched frame. Record actual viewport, source dimensions, crop,
projection and submission FOV. Identify whether the region exists before
replay, appears during replay, or appears only in D3D11 output. A source-image
gap alone does not prove CPU culling. Avoid changing native FOV or multiple
lighting/postprocess parameters without a discriminating observation.

Acceptance: demonstrated root cause and narrow fix, matched-image regression
check, then physical headset confirmation. No fix claimed yet.
