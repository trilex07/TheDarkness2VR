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
Runtime: pending. Simulator starts and title/main menu render; menu navigation
was unresponsive to native keyboard/mouse and acknowledged simulator controls.
Do not count delivery acknowledgments as gameplay validation.
Headset: NOT RUN. No performance improvement or full-game claim.

## Reproduction

Extract existing private DSW archives with tools/extract_shader_archive.py.
Run tools/audit-material-eyes.py with --shaders pointing at the private .bin
folder, --captures pointing at prior test-output and TEMP roots, --report to
private audit.json. --header can regenerate src/material_eye_allowlist.h only
after reviewing evidence; do not automatically admit future shaders in runtime.
Run tools/stereo-math-tests/build-reflection-eye.ps1 and
test/build_pixel_shader_metadata.ps1 before build.ps1 -DiagnosticCapture.
