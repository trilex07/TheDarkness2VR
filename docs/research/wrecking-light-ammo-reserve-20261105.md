# Wrecking crane specular eye and magazine/reserve HUD

MAIN, branch `codex/wrecking-light-ammo-reserve-20261105`.
Accepted predecessor `beace8e3` integrated/pushed to main at user request.
New work remains a separate candidate; headset acceptance NOT RUN.

## Crane material evidence

User identified the yellow cab beside the demolition ball in the current save.
Private diagnostic captures isolated individual draws by before/after RT pixel
differences. PS D94B61DC, two draws with VS1CB7CEE2, covers the crane including
cab and boom. Other material masks did not establish this ownership and were
not admitted. The crane animates independently: screenshots separated in time
must not be treated as a frozen-object visual A/B.

VS1CB7CEE2: 1884 bytes, SHA256
`e01892e5726d7533b21281e6ad8b11d40f860f25cfaf4181709bc879410ebcba`.
After bone deformation into r2, `add r1.xyz, -r2, c8` forms the eye vector;
tangent DP3s emit o3.xyz. c0..3 transform the same deformed position. c8.w is
animation time and is preserved. PS D94B61DC: 2124 bytes, SHA256
`c2ce27f5670b85dca4aeb06f8d02a02376a4a748699db813b6067a1bc21af2bc`.
It normalizes v2, reflects it using the normal, transforms through v3..5,
then dots against SHLight dominant light c64 for specular. It is not a HUD icon.

Reuse the existing draw-scoped eye correction only for this full-SHA pair.
Raw-history c8 ownership, native camera-center residual, perspective validity,
head-geometry and shadow-atlas rejection remain unchanged. Restore exact device
c8 after draw, including failure. Bones, lights, diffuse textures and other
materials are unchanged. Cube-map experiment remains OFF.

Private evidence: `test-output/wrecking-light-ammo-20261105/eye-consistency.json`.
Before (capture18152/115605): shader eye differs from effective WVP center by
0.032496 / 0.032503 game units for eyes0/1. These are per-draw consistency
measurements, not matched visual A/B at the same world state.
After (43164/115700,115701,115702): both eyes, yaw0/-0.25 radians and x0/0.25m;
maximum residual 0.000001641 game units. Both crane draws captured at each pose.
Runtime logs confirm scoped correction in live and replay eyes.

The overall matched-frame recorder reports incomplete staged_result; use the
selected draw-return, shader identity and effective VS-bank witnesses only.
Do not call this a complete-frame replay certification or headset visual PASS.
Large intermediate RT readbacks from material searches were discarded after
retaining hashes, metadata and derived pixel masks; private binaries not in Git.

## Ammo producer

Pinned EXE D46BB6CB. Gun vtable1010E30+148 -> BD2010, thiscall RET0C, three
int output pointers: magazine, capacity, reserve. Function sizeB9 SHA256
`8547c8b784ca9fb65b41727950171ad47b5a5c703321704caf85294f25e39018`.
First output invokes v154/6E8020; capacity v158/51BA40. Third gets ammo-resource
via gun vEC, gun+980 owner -> inventory v300 -> Has vA4/count vA8. Owner absence
leaves third output untouched, hence initialize -1. No guessed pool offset or
capacity-as-reserve fallback. Call from the existing owner-checked native
CaptureNativeGameplay path, inside its SEH guard, with code/vtable identity checks.

Publish reserve in the same locked loadout snapshot as magazine. Atlas cache
invalidates on either change, retains native-to-XR handedness and low-magazine
colour, draws unknown as --. Label is magazine/reserve, e.g. 7/11, shared native
ammo resource pool where applicable. Increase counter width from6cm to8cm.

Runtime diagnostic89540B8A: right5/30,left30/30; firing right changes4/30;
native reload changes right30/0,left30/0. Native engine owns magazine/pool
transfers; this mod does not adjust ammunition. Both quad layers submitted.
Simulator projection capture did not establish readability of hand-attached
labels; geometry preview inspected separately (7/11,32/9999,999/9999,--/--).
Final in-headset readability and subjective crane result remain NOT RUN.

## Validation and recovery

- PASS dual-wheel/HUD tests:137 checks, /W4 /WX; preview inspected.
- PASS actual production reflection scope: both eyes, exact restore, failed
  driver call, wrong VS/PS, native ownership, transformed geometry; new crane
  pair works with cube experiment OFF and rejects crossed pairs.
- PASS pixel metadata test: reused COM addresses, malformed code, concurrent
  publication, native HRESULT/bind semantics. Existing fixture compiler warnings.
- PASS protected-native and standard x86 compilation. FAIL standard/profile
  compatibility gate: existing native hands/camera/input/menu features require
  the protected-native build. Standard DLL rejected before deployment/package.
- Diagnostic DLL89540B8A39FD399ACAF678AE9E69E88447EE491E9E30FAEAA3AF40331CA10E22.
- Candidate uses the exact tested protected-native DLL89540B8A above, with the
  accepted profile unchanged and recording/simulator OFF. The standard
  DLL554CD74B was rejected and is not in the candidate.
- No FPS improvement claim; resolution/filtering unchanged.
- Own simulator process closed; exact DLL7DD55BA4 and diagnostic117230AC restored.
  Fresh four user save/config backups restored with SHA checks. Protected EXE,
  INI and loader verified. Accepted shrine/script assets unchanged.

Candidate preparation: `tools/prepare-wrecking-ammo-candidate.ps1` freezes one
DLL plus exact rollback, checks accepted predecessor and protected files.
Install.ps1 supports VerifyOnly and Restore; never launches the game.
