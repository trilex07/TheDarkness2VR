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

## After-fix gameplay, 2026-11-05

User selected Continue. PID26984 gameplay reached the crane and ball animation.
Captures920/57341 and921/57755 did not contain the selected particle PS;
they are NOT evidence of an absent visual defect. Captures924/62570 (yaw0)
and926/76994 (yaw-0.25) contain VS9E39DAF1 / PSDA655BE2, with successful
selected-before/after surface readbacks. Both eyes in both captures retain
c4=[.5,0,0,0;0,-.5,0,0;0,0,1,0;.500149488,.500146627,0,1].
tools/check-particle-projection-capture.py reproduces the four-draw PASS.
This confirms effective device constants in gameplay, beyond offline fixtures.
It does not certify the user's exact impact-cloud skyline artifact as resolved:
the selected effect changes size/opacity during animation and matched identical
impact phases were not captured. Headset assessment remains NOT RUN.

Material captures922/59453 and923/59968 contain two crane D94B61DC draws each.
Their effective SHLight c57..65 contains zero coefficients/color; c64=(0,1,0,0).
Consequently the dominant SH specular contribution is zero in those draws.
The shader also has ambient/specular terms, and later passes affect the image;
this does NOT rule out every material contribution. Do not claim a light-pass
producer, or a reflection fix, from the zero SH contribution alone.
Height-fog PS33DE98C1 also reconstructs height from depth-scaled NDC. Its
matrix convention requires separate proof before another shader patch.

Recovery PASS: closed only PID26984 after path plus UTC-start-tick identity
check. Restore verified E8C8015E DLL,117230AC diagnostic INI, protected files
and all four fresh pre-session save/config hashes. Emulator and recording OFF.
Private evidence: test-output/crane-smoke-20261105/recovery.json,
effective-particle-c4.json, after-banks.json, after-runtime.log.
