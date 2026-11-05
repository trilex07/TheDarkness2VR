# MAIN: wrecking-ball light and reserve ammo HUD

Branch codex/wrecking-light-ammo-reserve-20261105, .main-camera-shake.
Accepted baseline/main beace8e3 pushed to origin; accepted Steam DLL7DD55BA4,
diagnostic117230AC and shrine H.Misc pair per BASELINE.md. User authorized
simulator investigation using current save, no redeployment during main merge.

Scope: reproduce head-dependent vehicle light down the stairs beyond the
current shrine save; correct exact shader consumer only after evidence. Add
magazine/reserve display to both controller HUDs, preserving loadout routing.

Ammo evidence: pinned stock EXE D46BB6CB. Gun vtable1010E30+148 -> BD2010,
three int output pointers, thiscall RET0C. First output calls v154 (6E8020,
magazine clamped to capacity); second v158 (capacity); third obtains gun's
ammo-resource via vEC and owner980 -> inventory v300 -> Has vA4 / count vA8.
The third output is left untouched when owner absent, so initialize unknown -1.
BD2010 sizeB9 SHA8547c8b784ca9fb65b41727950171ad47b5a5c703321704caf85294f25e39018.
No guessed pool offsets or capacity-as-reserve fallback.

Implementation and simulator checks complete; headset acceptance NOT RUN.
Crane pair VS1CB7CEE2/PSD94B61DC identified by selected pixel-delta witness;
full-SHA gated scoped c8 eye correction, both eyes, yaw and translation verified.
Magazine/reserve native getter verified in gameplay through fire/reload.
Detailed evidence and limits: docs/research/wrecking-light-ammo-reserve-20261105.md.
Candidate DLL89540B8A, exact tested protected-native build; standard554CD74B
rejected by profile gate (omits required native features). Own emulator closed, exact accepted
Steam DLL7DD55BA4/diagnostic117230AC and fresh four user saves/configs restored.
New candidate is DLL-only; build/wrecking-light-ammo-reserve-20261105/Install.ps1.
Do not confuse source simulator PASS with physical headset acceptance.
