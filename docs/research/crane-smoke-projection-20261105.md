# Crane highlight and smoke depth follow-up

MAIN, branch codex/crane-smoke-projection-20261105, 2026-11-05.
User accepts magazine/reserve HUD; installed E8C8015E material-eye candidate
does NOT visually fix the crane highlight. Main acceptance is not implied.

## Particle defect

VS9E39DAF1,1948 bytes,SHA256
bf3a521ace33c7b509785dede723728b10a23b25d65bf85cab78e4f872beaa48:
c0..c3 produce clip r0, o0 receives r0; the next four instructions multiply
r0 by c4..c7 into o4. This is clip-to-texture, not world-to-texture. PSDA655BE2
samples DepthBuffer with o4 to soften particle intersections. CTAB name
ScreenTextureProjection alone incorrectly admitted a second head/eye transform.
Archive captures31736/861/3343 and862/3393 show c4 first row changing from
[0.5,0,0,0] to [0.6528,0.1500,0.3367,0.3367] with head yaw. The projected
position was already transformed, causing mismatched scene-depth coordinates.

Ten archived variants in screen_projection_input.h share the complete
four-instruction r0*c4 -> o4 path and o0=r0. Exact hash/length/SHA256/c4 gates
exclude only their second injection. c0 WVP, particle depth, textures and
softness remain native. Live/replay workers both see reflectedStp=-1.
Private classification: test-output/crane-smoke-20261105/postclip-audit.json.
No proprietary shader blobs committed. Manifest wvp_register records token
analysis, not necessarily the CTAB active register; its -1 does not prove that
WVP compensation was absent.

## Tests and current runtime

PASS x86 regression: all ten digest/identity gates, both eyes, three poses;
depth texture UV agrees with raster position and old double transform fails.
PASS existing production reflection-scope regression and protected-native build.
User selected Continue after automatic menu input failed. Emulator gameplay
reached crane. Before capture1636/910/56046 isolates PSDA655BE2 draw408,
109078 changed pixels. Capture911 selects crane PSD94B61DC. Exact impact-dust
ownership and after-fix visual effectiveness remain pending; no headset PASS.

## Reflection remains open

Existing camera-center agreement was insufficient by user feedback. Do not
expand the material allowlist or claim generic reflection repair from that test.
Requested selected-draw captures now also include effective PS bank for a
non-lighting material, allowing SHLight c57..65 inspection. Readback is bounded
to an explicit diagnostic request. Need isolate the actual moving highlight's
contributing draw and compare material/light/depth coordinates. Crane animation
makes casual different-time screenshot comparisons invalid as a stability proof.

Lab test-output/crane-smoke-20261105 preserves exact original E8 DLL/INI rollback
and four fresh user save/config backups. Own PID/ticks/path in process.json.
Recovery completion and candidate identity are recorded in the task handoff.
