# Plan: DLSS 4.5 Super Resolution / DLAA w Darkness2VR

**Task ID**: `dlss45-super-resolution`
**Autor**: SIDE (analiza offline, 2026-11-01)
**Wykonawca runtime**: MAIN (Steam, gogle, deploy)
**Status**: PLAN — nic nie zaimplementowano ani nie wdrożono
**Powiązany plan**: [PLAN-dlss5-feeder-integration-main.md](PLAN-dlss5-feeder-integration-main.md) (wspólna infrastruktura hosta x64)

---

## 1. Co to jest DLSS 4.5 i co z niego bierzemy

| Składnik DLSS 4.5 | Bierzemy? | Powód |
|---|---|---|
| **Super Resolution**: transformer 2. generacji (modele **M** i **L**, FP8) | **TAK** | Rekonstrukcja czasowa z niższej rozdzielczości do rozdzielczości oka |
| **DLAA**: ten sam model, wejście w natywnej rozdzielczości | **TAK, jako pierwszy cel** | Zastępuje wyłączone FXAA (`DisableFXAA=1`), daje stabilne krawędzie bez migotania |
| Model **K** (DLSS 4.0) | Jako fallback | Tańszy; do porównania kosztu |
| (Multi) Frame Generation | **NIE** | Generator 2D niezależny od stereo, dodaje opóźnienie i wymaga własnego Present/Reflex. W VR klatki pośrednie robi runtime (reprojekcja SteamVR/VD/PSVR2). |
| Ray Reconstruction | **NIE** | Gra nie ma ray tracingu |

**Sprzęt**: RTX 5070 (Blackwell) obsługuje FP8 natywnie, więc modele M/L kosztują tu wg testów ~2–3% FPS, nie 14–24% jak na RTX 20/30.

### Ważne zastrzeżenie wydajnościowe
Recon eye trackingu opisuje obecną klatkę jako **CPU-bound** (replay SubmitMode 4).
- W scenach CPU-bound **upscaling nie doda FPS**, bo GPU i tak czeka. Wtedy zyskiem jest tylko **jakość** (DLAA).
- Upscaling (Quality/Balanced) da FPS tylko tam, gdzie wąskim gardłem jest GPU, np. profile 5K (`5k90-*`).
- Dlatego pierwszy cel to **DLAA**, a tryby upscale są drugim krokiem, uzależnionym od pomiarów.

---

## 2. Twarde ograniczenia (zweryfikowane w kodzie)

1. **NGX/DLSS jest wyłącznie x64.** Gra i nasz `d3d9.dll` są x86. OpenXR (`xrCreateSession` d3d9.cpp:5137) i D3D11 (`g_dev11`, d3d9.cpp:1423) działają w procesie x86.
   → DLSS musi działać w **osobnym procesie x64**. Dostaje tekstury współdzielone (NT handle) i współdzielony `ID3D11Fence`.
2. Obecny mostek x64 (`src/psvr2/BridgeMain.cpp`) obsługuje tylko PSVR2 (triggery, gaze) przez jednokierunkowy pipe i **nie nadaje się do tekstur**. Potrzebny jest nowy host.
3. **`nvngx_dlss.dll` nie trafia do Gita** (licencja NVIDIA). Plik dostarcza użytkownik, np. z instalacji innej gry lub przez DLSS Swapper. Host ładuje go ze ścieżki z INI. Nagłówki SDK (github.com/NVIDIA/DLSS) dodać zgodnie z ich licencją.
4. Host musi używać **tego samego adaptera (LUID)** co `g_dev11`. W systemie jest też iGPU AMD.

---

## 3. Wejścia DLSS SR i ich stan u nas

| Wejście | Wymaganie DLSS | Stan w Darkness2VR | Luka |
|---|---|---|---|
| Kolor | aliasowany, przed upscalingiem | Obrazy oczu `B8G8R8A8_UNORM` w `NisEyeProcess()` (`src/nis_eye.inl`). FXAA i DOF już wyłączone. | Obraz jest **po** tonemappingu i post-processingu, a pewnie też z HUD-em. Do sprawdzenia, czy HUD jest w obrazie oka. |
| Rozdzielczość | wejście < wyjście (SR) lub = (DLAA) | `RenderWidth/Height` (d3d9.cpp:850), swapchain `g_scW/g_scH` (d3d9.cpp:1508/5368). Ścieżka NIS mode 3 już robi „wejście → wyjście swapchainu”. | Brak. To gotowy slot. |
| Głębia | pełna rozdzielczość wejścia | `g_intzTex9` (INTZ/RAWZ/DF24, d3d9.cpp:2484–2556), ale kopiowana tylko do **480×270** `g_depthSRV11` | Pełna rozdzielczość **per oko** w replayu |
| Wektory ruchu | RG16F/RG32F, piksele lub znormalizowane, kamera + obiekty | **Brak.** Jest shader MotionBlur VS 73EBA724 (c0 bieżące WVP, c4 PrevWorldProj). RT/format nieznane. | Największa luka |
| Jitter | sekwencja subpikselowa (Halton) w projekcji + wartości przekazane do DLSS | **Brak.** Projekcję edytujemy w `ApplyEyeShiftToMatrix` (stereo_replay.inl:337, wywołanie 1294), ale tylko dla zidentyfikowanych WVP | Jitter musi być spójny we **wszystkich** przebiegach (deferred lighting, cienie). Patrz FAILED-APPROACHES: „c0..c3 shear alone insufficient for deferred”. |
| Reset historii | przy cięciach | Mamy zdarzenia: menu, F10, zmiana SubmitMode, Reset urządzenia | Trzeba je podpiąć |

---

## 4. Etapy z bramkami (GO/STOP)

Etapy 0–2 to praca **offline** (SIDE). Etapy 3–6 to **runtime** (MAIN). Wszystkie zmiany są domyślnie wyłączone w INI.

### Etap 0: rozpoznanie SDK (offline)
- Pobrać DLSS SDK (nagłówki, dokumentacja „DLSS Programming Guide”). Zapisać w `docs/research/dlss45-sdk-recon.md`:
  - wymagane i opcjonalne parametry `NVSDK_NGX_Feature_SuperSampling` (D3D11);
  - format i skalę MV, konwencję jittera;
  - flagi: `MVLowRes`, `IsHDR=0` (LDR po tonemappingu), `DepthInverted`, `AutoExposure`;
  - wybór presetu M/L/K przez parametry renderowania;
  - zachowanie przy braku jittera i przy MV = 0.
- Sprawdzić licencję nagłówków i warunki dla `nvngx_dlss.dll`.
- **STOP**, jeśli D3D11 nie jest już wspierane albo licencja blokuje taki użytek.

### Etap 1: host x64 i test na zapisanych klatkach (offline)
- `tools/dlss-host64/` (x64, D3D11, NGX): ładuje DLSS z katalogu użytkownika.
- Wejście: prywatne zrzuty z matched capture (RT0/final-eye). Sekwencje klatek, jeśli dostępne; inaczej klatki statyczne.
- Pomiary (timestampy jak w `nis_runtime.h`):
  - DLAA i Quality, modele M i K, rozdzielczość oka ~2676×2728 oraz profil `5k90-scaled` (2048×2091);
  - **jeden przebieg na oko**, czyli koszt pary.
- Porównanie wizualne z NIS (mode 2/3) na tych samych klatkach. Metryki L/P: `test/pair_diff.py`, `test/eye_diff.py`.
- **GO**, jeśli para DLAA mieści się w ~2,5–3 ms GPU na RTX 5070 i obraz jest wyraźnie lepszy od NIS.

### Etap 2: interop x86↔x64 (offline; fixture jak testy NIS/WARP)
- Testowy exe x86: dwie tekstury wejściowe i dwie wyjściowe z `D3D11_RESOURCE_MISC_SHARED_NTHANDLE`, współdzielony `ID3D11Fence`, wybór adaptera po LUID.
- Host: `OpenSharedResource1`, `OpenSharedFence`. Strona gry: `ID3D11DeviceContext4::Signal/Wait`, czyli **bez blokowania CPU**.
- Testy: zgodność pikseli, 1000 cykli, zabicie hosta (gra przechodzi na NIS z timeoutem i nie wisi na fence), restart hosta, odmowa na iGPU.
- **Tę samą infrastrukturę wykorzysta później plan DLSS 5.**

### Etap 3: DLAA bez jittera i MV kamery (pierwszy test w grze)
Najmniej inwazyjny wariant, bo nie zmienia renderingu gry:
1. Wejście = obecne obrazy oczu w natywnej rozdzielczości (DLAA). Wyjście trafia do swapchainu XR w miejscu, gdzie dziś działa NIS.
2. **Pełna rozdzielczość głębi per oko** z `g_intzTex9` w replayu. Udokumentować, kiedy INTZ zawiera głębię prawego oka.
3. **MV kamery**: compute shader rekonstruuje ruch z głębi oraz bieżącej i poprzedniej macierzy widok-projekcja **danego oka**, czyli pozy oka z OpenXR i FOV z submitu.
4. Osobny feature/kontekst DLSS **dla każdego oka**, z osobną historią.
- Oczekiwanie: stabilniejsze krawędzie, ale bez pełnego zysku detalu, bo nie ma jittera. Możliwe smużenie na animowanych obiektach.
- **Bramki**: komfort stereo, brak smużenia przy obrocie głowy, koszt GPU, porównanie A/B z NIS.

### Etap 4: wektory ruchu obiektów (RE, zasada „evidence before patch”)
- Zbadać MotionBlur 73EBA724: jaki RT, format i rozdzielczość pisze, czy obejmuje skinned meshes (macki, wrogowie), czy działa w replayu prawego oka.
- Dowody: hash shadera, zrzut RT z matched capture, adresy i bajty, jeśli potrzebny patch.
- Jeśli bufor ruchu istnieje: przekazać go do DLSS, przeskalowany per oko. Jeśli to tylko blur kamery, zostaje MV kamery z Etapu 3 i ryzyko smużenia dla animacji.

### Etap 5: jitter (warunkowy; najwyższe ryzyko)
- Dodać subpikselowe przesunięcie w przestrzeni clip (`clip.xy += j * w`) do **każdej** macierzy projekcji, nie tylko zidentyfikowanych WVP. Musi też obejmować przebiegi deferred, które rekonstruują pozycję z głębi przez odwrotną projekcję.
- Najpierw RE i lista shaderów oraz rejestrów z projekcją. Bez tej listy **nie wdrażać**: niespójny jitter da drgające oświetlenie i cienie.
- Jeśli jitter jest niemożliwy do pokrycia w 100%, zostajemy przy DLAA bez jittera.

### Etap 6: tryby upscale (tylko jeśli GPU jest wąskim gardłem)
- Wejście `RenderWidth/Height` < rozdzielczość swapchainu (jak NIS mode 3), presety Quality/Balanced, model M.
- Ma sens tylko w profilach 5K. Bramka: zmierzony zysk FPS przy braku regresji komfortu.

---

## 5. Integracja w kodzie (Etap 3+)
- Nowy `src/dlss_eye.inl` obok `nis_eye.inl`, wywoływany w tym samym miejscu. Wyklucza się z NIS: albo DLSS, albo NIS.
- Proponowane klucze INI (**propozycja**, domyślnie wyłączone):
  ```ini
  [Render]
  DlssMode=0            ; 0=off (NIS jak dziś), 1=DLAA, 2=Quality, 3=Balanced
  DlssPreset=M          ; M / L / K
  DlssHost=host64\d2vr-dlss-host64.exe
  DlssDllDir=           ; katalog z nvngx_dlss.dll dostarczonym przez użytkownika
  DlssJitter=0          ; Etap 5, tylko po RE
  ```
- Sticky fallback jak w NIS: błąd hosta, timeout fence, zły adapter albo brak DLL → log `[DLSS] ... disabled until device Reset` i ścieżka NIS w tej samej klatce.
- Logi: koszt GPU pary (próbka/peak co 120 klatek), preset, rozdzielczości, LUID, licznik resetów historii.

## 6. Bramki w goglach (MAIN)
| Bramka | Kryterium |
|---|---|
| G1 | Host uruchomiony, feature utworzony, LUID zgodny (`d3d9.log`) |
| G2 | Brak różnic L/P i dyskomfortu (restauracja, walka z mackami) |
| G3 | 90 Hz bez reprojekcji w scenie referencyjnej; koszt pary w logu |
| G4 | Brak smużenia przy szybkim yaw, przy mackach i przy HUD |
| G5 | Zabicie hosta w trakcie gry → NIS bez zawieszenia |
| G6 | `DlssMode=0` → obraz 1:1 jak dziś |

## 7. Rollback
`DlssMode=0` (domyślnie). Standardowy backup `build.ps1 -Install` (`d3d9.dll.off-<timestamp>`); katalog `host64\` usunąć ręcznie.

## 8. Szacunek
| Etap | Kto | Czas |
|---|---|---|
| 0 SDK recon | SIDE | 0,5 dnia |
| 1 host + test na klatkach | SIDE | 1–2 dni |
| 2 interop | SIDE | 1–2 dni |
| 3 DLAA w grze (głębia, MV kamery) | SIDE→MAIN | 2–3 dni |
| 4 MV obiektów (RE) | MAIN/SIDE | 2–5 dni |
| 5 jitter (RE) | MAIN/SIDE | 3–7 dni, może się nie udać |
| 6 upscale | MAIN | 1 dzień |

## 9. DLSS 4.5 vs DLSS 5 (decyzja)
| | DLSS 4.5 SR/DLAA | DLSS 5 Neural Rendering |
|---|---|---|
| Efekt | Ostrość, antyaliasing, opcjonalnie FPS | Zmiana oświetlenia i materiałów |
| Koszt | Niski na RTX 50 (FP8) | Wysoki, dodatkowy |
| Styl komiksowy | Zachowany | Zagrożony |
| Ryzyko stereo | Niskie (deterministyczna rekonstrukcja) | Wysokie (generatywne) |
| Wspólne | Host x64 + interop + głębia + MV | jak obok |

**Rekomendacja**: najpierw DLSS 4.5 (DLAA), a potem ewentualnie DLSS 5 na tej samej infrastrukturze.
