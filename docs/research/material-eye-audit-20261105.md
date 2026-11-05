# Material eye audit, 2026-11-05

Scope: extend the exact crane consumer correction to other proven material
variants. This checkout inherits crane/ammo candidate cf4c3869, not accepted
main beace8e3. Source branch codex/material-eye-audit-20261105.

## Collected evidence

Private local archive: test-output/material-eye-audit-20261105/audit.json.
1427 distinct previously collected shaders (261 vertex, 1166 pixel); 203
previous matched-frame manifests. These are encountered/preloaded caches,
not every shader or location in the game. No game bytecode is committed.

40 vertex shaders subtract model-space surface position from c8 eye position.
21 pass the conservative tangent-interface filter: c8 subtraction, no intervening
xyz overwrite, all three tangent DP3 components emitted to TEXCOORD1/o3,
and no local definition of c8. Of these, 13 occur in captured compatible pairs.
Consumers normalize TEXCOORD1/v2 and have Shine or ReflectionMap metadata.
184 observed VS/PS pairs use 13 VS and 176 PS. Other combinations are excluded;
matching an individual shader never authorizes its unobserved cross products.

Production list pins FNV identity, exact byte count AND full SHA-256 for every
shader, then checks the exact observed pair. Draw-scope correction additionally
requires head geometry, a non-shadow pass, complete native WVP/c8 history,
actual/raw c8 equality, and native c8 matching the raw WVP camera center.
Correction uses the actual eye WVP camera center; c8.w and all original values
are restored. The broader path includes proven cube-map consumers even with
the legacy two-consumer ReflectionEyeExperiment switch OFF. It does not enable
that legacy experiment globally.

Audited VS: 12CCC8DB,19D05250,1CB7CEE2,1E6B1A39,293F456B,454C74CB,
7F4E0755,84A9E5F4,8EB68C19,DDB52E69,E2AA099B,F1403643,F839CD9B.
Numbers describe shader pairs, not a counted number of visible objects.
Structural bytecode evidence is not subjective visual or full-game acceptance.

## Validation

PASS: production ReflectionEyeScope extracted into CPU fixture, both eyes,
new audited pair with cube experiment OFF, unobserved cross-pair rejection,
wrong size/SHA rejection, original register restoration, setter/read failures,
native ownership, transformed/nonuniform geometry; all pinned pair lookups.
PASS: actual-source pixel metadata lifecycle and concurrent publication fixture.
PASS: protected-native x86 build using build.ps1 -DiagnosticCapture.
PASS: git diff --check and Python compile.
Runtime follow-up PASS after user selected Continue. Three actual gameplay
captures per build, both eyes: yaw=0/x=0, yaw=-0.25/x=0, yaw=-0.25/x=0.25m.
Private retry lab: test-output/material-eye-retry-20261105T142053.

The first build CEAFF3BC exposed a real ordering conflict: ReflectionEyeScope
ran before TentacleHeadScope. The latter changes WVP without updating c8;
9 left-eye attached-tentacle draws after head translation had camera-center
residual ~0.25 game units (PS1A778906,CC538CB9,891DD2C1 / VS1CB7CEE2).
All four primitive draw wrappers now construct ReflectionEyeScope AFTER
TentacleHeadScope, so c8 is based on the final matrix and restores first.
No attachment/geometry validity gates were bypassed.

Final protected-native DLL E8C8015E58045B6F8EA2FE744E142540FEB478608F40D3FB508B8DB4B484C72B:
captures31736/861/3343,31736/862/3393,31736/863/3445.
790 admitted draw witnesses,53 observed pairs,8 VS families, both eyes.
Maximum independent float64 inverse vs captured c8 residual0.000138057;
all below the existing native ownership tolerance0.001 (sqrt1e-6).
VS1CB7CEE2 residual after ordering fix <=0.000002781 in these captures,
including the translated pose. Numerical residual is in model/game units,
not physical headset meters. Private eye-consistency-after.json stores all draws.
The earlier private report used a stricter exploratory1e-4 flag; final contract
comparison explicitly uses the already existing1e-3 ownership tolerance.

The new scene exercises8 of13 admitted VS and53 of184 pairs. Other admitted
identities remain offline-only. Matched capture staged_result is incomplete;
these are individual draw/register witnesses, not a certified complete frame.
Visual emulator image inspected for hands, tentacles and scene; no obvious
corruption observed. No identical-pose pixel A/B or physical headset assessment.
CPU/FPS regression has not been isolated from game/driver pacing in this test.
Headset: NOT RUN. No performance improvement or full-game claim.

## Reproduction

Extract existing private DSW archives with tools/extract_shader_archive.py.
Run tools/audit-material-eyes.py with --shaders pointing at the private .bin
folder, --captures pointing at prior test-output and TEMP roots, --report to
private audit.json. --header can regenerate src/material_eye_allowlist.h only
after reviewing evidence; do not automatically admit future shaders in runtime.
Run tools/stereo-math-tests/build-reflection-eye.ps1 and
test/build_pixel_shader_metadata.ps1 before build.ps1 -DiagnosticCapture.


Recovery PASS: own simulator PID31736 closed after identity/start-time check;
Steam DLL7DD55BA4 and diagnostic117230AC restored, protected files unchanged;
all four fresh user saves/configs restored and hashes verified. Simulator OFF.
The tested DLL is preserved as a separate candidate, not installed or merged.
