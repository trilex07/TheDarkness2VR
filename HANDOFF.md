# Darkness2VR — HANDOFF v0.5 (2026-10-18)

## CIĄGŁY TASK (od użytkownika): pełna dekompilacja silnika i renderowania
„Dekompilować grę aż będziesz wiedział wszystko o niej — silniku i renderowaniu!"

**Program RE (priorytety):**
1. **Rozdzielczość renderu** — RT0 (G-buffer) jest zawsze 1920×1071 niezależnie od
   backbuffera (wymuszone 5120×2880 nie zmieniło go). Znaleźć funkcję/global, który
   rozmiaruje render targety → wymusić 5K. (W toku: agent RE, RE_FINDINGS.md)
2. **Pipeline renderowania** — mapa przebiegów (G-buffer → deferred lighting → cienie →
   post), adresy funkcji, kiedy co jest renderowane.
3. **FOV + culling** — jak `mBaseFovOverride`/`gFOV` trafia do macierzy projekcji i gdzie
   budowany jest frustum cullingu (problem „góra/dół nie renderowana").
4. **StateBlock vtable** — potwierdzić układ runtime'u (artefakty: silnik wiąże shadery
   przez StateBlock::Apply; hook z walidacją `ret N` jest w kodzie, domyślnie włączony).
5. **Pełna mapa rejestrów shaderów + pipeline deferred** (CTAB + HelixFix HLSL + bytecode).

**Materiały RE (gotowe):**
- `thunk_disasm.txt` — pełny disasm DarknessII.exe (dumpbin, 221MB, adresy VA).
- `test-output/shaders/*.bin` — bytecode shaderów z gry + `tools/vs_analyze.py` (parser
  tokenów SM3 + CTAB z nazwami semantyk!).
- `helix_fix/` — źródłowy asm HLSL silnika (LitInstance = WVP c0..c3, World c4..c7,
  ScreenTextureProjection c8..c11, ModelSpaceEye c12) + fix HelixMod (wzór shear).
- `ENGINE_REVERSE_ENGINEERING.md` — mapa metod Lua kamery (thunki), Region::GetPlayerAvatar
  (0x00B73EE0, ret 8), macierzowe funkcje 0x00C02F50 / 0x00D46710, tabela semantyk
  0x00712800, singleton D3D9 0x010dd394.
- `extracted_assets/` (H_Script 547, B_Script 475) — skrypty Lua + shadery z cache gry.

**Zasady RE (z AGENTS.md):** dowód przed patchem — każdy claim = adres + bajty/instrukcje,
które go dowodzą. Nie zgadywać rejestrów/offsetów.

---

## Cel: pełne VR dla The Darkness II (Evolution Engine, D3D9Ex, x86, bez ASLR)

Objawy zgłoszone przez użytkownika (start sesji v0.5):
1. „Niby 3D zamiast prawdziwego stereo" — stary pipeline (Method 2) to przesunięcie UV
   ekranu 2D bez prawdziwej głębi.
2. „Nie czuć głębi" — brak realnej dysparycji geometrycznej.
3. „Obszary poza sceną nie renderowane (góra/dół)" — klatka ma tylko ~73°x45°, a gogle
   pokazują ~105°; geometria poza frustum gry nie istnieje (CPU culling + FOV).

---

## Co zostało zrobione w v0.5 (ten commit)

### 1. Pełna analiza RE silnika (dokumentacja + narzędzia)
- `ENGINE_REVERSE_ENGINEERING.md` — mapa rejestrów shaderów (tabela semantyk `0x00712800`),
  adresy metod kamery `BaseAvatar` (SetView/GetView/GetEyePosition/SetEyePosition/GetFOV/
  SetBaseFovOverride=0x00dbdb90), Region::GetPlayerAvatar=0x00b73ee0, singleton D3D9.
- `tools/vs_analyze.py` — analizator bytecode'u SM1-3 (działa też na plikach z helix_fix).
- `helix_fix/` — oryginalny fix HelixMod (3D Vision) z **źródłowym asm HLSL** silnika:
  potwierdza `LitInstance { WorldViewProjection c0..c3, World c4..c7,
  ScreenTextureProjection c8..c11, ModelSpaceEye_Time c12, ... }`, konwencję **row-vector**
  (`clip = x*c0 + y*c1 + z*c2 + c3`), wzór shear `x += sep*(w-conv)`.
- Potwierdzone: paleta kości `c45+` (68 kości), INTZ depth nieczytelny w Present (DIBR martwy),
  AFR odrzucone przez użytkownika, prezentacja przez **SwapChain::Present**.

### 2. Nowy pipeline: TRUE same-frame stereo (SubmitMode=4)
Pliki: `src/d3d9.cpp` + `src/stereo_replay.inl` (nowy).

- **Nagrywanie strumienia device calls** (29 hooków: stany + drawe + stałe) do aren.
- **Replay dwukrotny**: oko 0 = render żywy gry (shear +sep), oko 1 = replay całego
  strumienia z shear -sep. Zero AFR, zero DIBR — obie oczy z **tej samej klatki**.
- **Wstrzykiwanie shear stereo** `clip.x += sep*(clip.w - conv)` składane w macierz:
  `c_i.x += sep*c_i.w`, `c3.x -= sep*conv` (identycznie jak HelixMod — działa z deferred,
  cel-shadingiem i głębią, bo clip.z/w nietknięte).
  - Strategia A: analiza bytecode'u VS (`CreateVertexShader@91`) → dokładne rejestry
    WVP/STP per shader.
  - Strategia B (fallback, domyślna): semantyka silnika — upload od c0 z `count>=4` to
    per-object WVP; `count>=12` zawiera STP na c8..c11. Bloki 2D/ortho pomijane
    (test kolumny w: `len(c0.w,c1.w,c2.w) > 0.02`).
- **Submit OpenXR**: warstwa PROJECTION, obie oczy z shared pose + FOV z gry
  (`GameFovX/Y`), eye-cachye 1:1 (blit).
- Hotkeyi: F6 = separacja (StereoSep), F11 = zbieżność (StereoConv), F10 = SubmitMode 0-4.

### 3. Stabilność (krytyczne znaleziska)
- **SwapChain::Present NIE jest na slocie 3** vtable runtime'u gry — hookowanie go
  psuło wynik prezentacji i zatrzymywało render. Frame boundary = EndScene (VrFrame).
  Wniosek: nie patchować vtable obiektów spoza device bez weryfikacji runtime.
- **NVIDIA driver rzuca 0xC0000002 przy `UpdateTexture` z martwymi teksturami** podczas
  replayu (gra tworzy i zwalnia tekstury w obrębie klatki). Rozwiązanie: SEH-guard na
  każdy rekord replayu (`replay: exception ... skipped`) + `SafeMemcpy` w nagrywaniu.
- **Wątki**: strumień nagrywania zabezpieczony spinlockiem (silnik woła z >1 wątku).
- VEH loguje **pierwszą szansę** wyjątków >= 0xC0000000 (także łapanych przez nasze
  `__except`) — linie „EXCEPTION ... first-chance" to NIE crashe gry.

### 4. FOV / culling (w toku — KOLEJNY KROK)
- `TryApplyEngineFovOverride()` w `src/d3d9.cpp`: skan `.data` za Regionem
  (`[region+0xac]/[0xb0]` = tablica awatarów), walidacja awatara przez `GetFOV` thunk
  (0x00dc4740), `SetBaseFovOverride(0x00dbdb90)` + weryfikacja + przeliczenie
  frustum OpenXR (FOV pionowe w stopniach → `GameFovX/Y`).
- **Status**: na razie `avatar not found` (wzorzec struktury do weryfikacji), a adres
  singletonu D3D9 z dokumentu (`0x010dee84+0x0c`) NIE zgadza się w runtime
  (`= 000000C9`) — wymaga ponownego RE (disasm `0x00cf2c30`/`0x00438250`,
  weryfikacja rejestrów w `0x00946b80`).
- FOV gry = **pionowe stopnie** (dowód: `Py = 1/tan(FOV/2)`, FOV=45 → Py=2.414 ✓).
  `SetBaseFovOverride(100)` da ~100° pion / ~129° poziom → pokrycie HMD + culling ✓.

---

## Konfiguracja (Darkness2VR.ini)
```
SubmitMode=4        ; VR_NATIVE_STEREO (domyślne)
StereoMode=1        ; 1 = pełne stereo, 2 = walidacja (obie oczy identyczne), 0 = mono
StereoSep=0.020     ; separacja (shear) - F6
StereoConv=2.0      ; zbieżność w jednostkach świata - F11
StereoPolarity=0    ; 0 = lewe oko +shift
GameFovX=73.2       ; FOV obrazu gry (stopnie, pełne) - do projekcji OpenXR
GameFovY=44.9
FovOverride=100.0   ; natywny override FOV silnika (0 = off) - wymaga poprawnego RE
HookMask=15         ; diagnostyka: bit0=record, bit1=swapchain(WYL), bit2=analiza VS, bit3=constF
```

## Następne kroki (priorytet)
1. **Test użytkownika w HMD** (SubmitMode=4, StereoMode=1, wejdź do gry):
   potwierdzić realną głębię (F6 = separacja, F11 = zbieżność), log: `shifts=N > 0`.
2. **FOV/culling**: poprawić RE (Region/avatar) → `SetBaseFovOverride(100)` →
   wypełnienie góry/dołu sceny. Alternatywa: GPU-skalowanie WVP (pewne, ale culling
   zostaje).
3. **6DoF**: hook `BaseAvatar::SetView` (tabela Lua w `.data`, para `0x10b9800`) +
   `SetEyePosition` — natywna kamera zamiast wstrzykiwania myszy.
4. Optymalizacja: zero-copy D3D9Ex shared textures (teraz 2x CPU readback/klatkę).
5. Ciche wyjątki first-chance (obecnie do 3 na kod).

## Znane ograniczenia v0.5
- Brak weryfikacji HMD dla nowego pipeline'u (wymaga testu użytkownika).
- Replay pomija `UpdateTexture` z martwymi teksturami (efekt: pojedyncze przebłyski
  wideo/UI w oku 1 przy klatkach z temp-teksturami).
- Analiza bytecode'u VS na razie nie trafia wzorców (wzorce łańcuchów do dopracowania);
  działa fallback semantyczny.
- FOV override nieaktywny (RE niedokończone) — brak pokrycia góry/dołu.


## MAIN offline integration - 2026-10-24

Branch `codex/integration-frame-capture` integrates the approved Luna/Sol/Astra
series, with no Gemini adapter and no merge into research. See
[matched-frame-capture](docs/research/matched-frame-capture.md) for capture
protocol, tests, native-patch isolation, exact integrated hashes and blockers.
The original EXE reappeared and passes the expanded read-only audit. Bitdefender
resolution is not yet confirmed. No Steam launch/deployment or headset test.
The native-patch-free diagnostic also needs proof of COM Present observation;
it refuses capture without that boundary. Native-wide FOV remains unimplemented.


## Gemini toolchain acceptance - 2026-10-24

Full series through 120b9e0 integrated on codex/integration-frame-capture,
followed by MAIN fixes for opaque SetViewport payloads (UNKNOWN) and unknown
VS/PS register state after ApplyStateBlock. Adapter suite: 25/25 PASS including
private trace; matched-capture tests 10/10; audit parser tests 6/6; Luna self-test
PASS. No production src/INI/build changes relative to 9c9496f, no Steam changes.
See external-toolchain-evaluation.md section 9 for exact picks and limitations.
This diagnostic adapter does not establish player-camera ownership. Next scope
remains same-frame RT0/eye capture and camera-to-visibility investigation.

## Runtime readiness follow-up - 2026-10-24

Branch codex/runtime-camera-readiness starts at integration 9fe7944. See
[runtime readiness receipt](docs/research/runtime-readiness-20261024.md).
Capture schema 2 requires a current Present witness and original S_OK return;
diagnostic COM coverage still excludes the engine's actual SwapChain Present.
Same worker matrix is now traced through 607CD0/62DD40 plane construction to
93F2F0/48B7F0 CPU bounds rejection and BC9230 index-list filtering. Active player
camera ownership, VS WVP correlation and scene-wide visibility remain open.
Original EXE re-audit passes 14 sites/18 ranges after user confirmed AV resolved.
One Steam launch started the client but no game process; test BLOCKED. DLL/INI
and preexisting diagnostic INI restored with hashes verified. No matched capture
or headset claim. User considering reboot; repeat identity audit before retry.
Local x86/D3D11 xrsim preflight verified only; no runtime selection changes.
