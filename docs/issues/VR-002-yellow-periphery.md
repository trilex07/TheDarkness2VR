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

2026-10-25 evidence: yellow region already visible at right edge of captured
left-eye D3D9 RT0. Final-D3D11-only origin ruled out for this region. Capture
incomplete; exact D3D9 pass/geometry/background cause remains unproven.
See ../research/staged-capture-20261025.md. No FOV/lighting change made.

Complete frame38224, seven intermediate images: yellow doorway absent from HDR
color through draw75 and half-resolution color through94, present in final
source and eye. Its introduction is narrowed to later postprocessing, not final
D3D11 crop. Exact late shader/input cause remains unproven; no CPU-culling claim.
Bounded CapturePassStages=2 targets the composite's actual Frame/Depth/DOF
inputs and PS constants. Complete frame28216 localizes the bright yellow region
to final composite draw135 (declared PS33DE98C1, actual PSA69BDC80). Its DOF RGB
bypass is active in the actual bytecode. Depth input format/pixels remain unknown;
unsupported input identities will now be reported explicitly. Native camera
ownership/visibility remains unproven; no speculative fog or FOV correction.

Targeted rendering candidate: `[Render] DisableCompositeHeightFog=1` (default0,
restart required) removes only the height-fog RGB blend in exact PS33DE98C1.
The blend at token340 consumes the reconstructed-depth fog weight and
HeightFogColor c5, then feeds the preserved HDR/bloom/grade chain. This is a
reversible effect bypass, not a proven correction of the depth/visibility cause.
It may reveal a dark geometry gap instead of the yellow fill, and deliberately
removes this atmospheric effect throughout this one composite's image.
Other shader hashes, FOV, culling, stereo, depth buffers and lighting are unchanged.
With fog bypass OFF, the DOF-only shader is byte-identical to the captured
A69BDC80. Both options ON produce C0A6F934; the actual D3D9 driver accepts it.
Steam/PSVR2 result for this candidate: NOT RUN; no installation changes.

Follow-up physical test of81dee71,2026-10-25: the user reports no yellow area,
apparently a black gap instead, with objects disappearing at the visual periphery.
The exact replacement C0A6F934 was created successfully and appears in the
saved draw606 shader. Thus keep the useful fog bypass candidate, but do not
close the visibility defect or claim CPU culling as its cause. The partial
source image shows a peripheral gap and both tentacles, but also a pause menu.
The single capture26047 is INCOMPLETE (mask3, no final eyes/COMPLETE); no retry
was made. A quantitative matched-frame comparison is invalid for this attempt.
The actual depth input is now identified as3076x3140 INTZ, not readable by the
existing raw-surface saver; depth pixels remain unavailable. Original DLL and
both INIs were restored with verified hashes after normal exit at15:40:17 local.
