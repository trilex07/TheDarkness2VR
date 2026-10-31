# Hi-Res Texture Pipeline (2K / 4K / 8K) — The Darkness II

**Branch**: `side/texture-hires-extraction-2k4k8k`
**Role**: SIDE prepares (offline); MAIN executes (Steam exclusive, game-dir access)
**Focus**: weapons, tentacles, enemies

---

## 1. Co SIDE przygotował (już w repo)

| Plik | Przeznaczenie |
|------|---------------|
| `tools/prepare_hires_textures.ps1` | Skrypt automatyzacji: ekstrakcja z pakietów gry → sortowanie → struktura docelowa |
| `tools/darkness2.bms` | QuickBMS skrypt do wypakowywania tekstur z `toc`/`cache` |
| `quickbms.zip` | QuickBMS 0.12.0 (wymagany do ekstrakcji) |
| `extracted_textures/source/{weapons,tentacles,enemies}/` | Źródłowe tekstury wyekstrahowane z gry (bronie: 62, macki: 36, przeciwnicy: 109) |

Źródłowe tekstury to PNG z sufiksami: `_d` (diffuse), `_n` (normal), `_s` (specular), `_e` (emissive), `_o` (occlusion), `_mask`.

---

## 2. Co MAIN musi zrobić (krok po kroku)

### Krok A — Zainstaluj narzędzia (tylko MAIN, na swoim komputerze)

1. **QuickBMS** — już w repo (`quickbms.zip`). Wypakuj `quickbms.exe`.
2. **Graficzne AI do upscalingu** — wybierz jedno:
   - **Real-ESRGAN** (darmowy, otwarty, najlepszy stosunek jakości/łatwości)
   - **waifu2x** (darmowy, dobry do stylizowanych tekstur)
   - **Topaz Gigapixel AI** (płatny, komercyjny)
   - **Upscayl** (darmowy, GUI na Real-ESRGAN)
3. **Narzędzie do DDS** — np. GIMP + wtyczka DDS, Paint.NET, lub NVIDIA Texture Tools (do konwersji końcowej na DDS z mipmapami).

### Krok B — Uruchom skrypt ekstrakcji i sortowania

W folderze repo (`Darkness2VR\`):

```powershell
# 1. Ekstrahuj tekstury z pakietów gry (wymaga dostępu do folderu gry Steam)
.\tools\prepare_hires_textures.ps1 -Mode Extract -GameDir "C:\Program Files (x86)\Steam\steamapps\common\Darkness II"

# 2. Posortuj wyekstrahowane tekstury do kategorii
.\tools\prepare_hires_textures.ps1 -Mode Sort -SourceDir "<folder-wyjścia-z-kroku-1>"

# 3. Przygotuj strukturę docelową 2K/4K/8K i manifest CSV
.\tools\prepare_hires_textures.ps1 -Mode Prepare -SourceDir "<folder-wyjścia>" -DestDir ".\extracted_textures"
```

Wynik w `extracted_textures/`:
```
extracted_textures/
├── source/
│   ├── weapons/     (62 pliki źródłowe PNG)
│   ├── tentacles/   (36 pliki źródłowe PNG)
│   └── enemies/     (109 pliki źródłowe PNG)
├── 2k/
│   ├── source/      (kopia źródeł do upscalingu 2K)
│   ├── weapons/     (tu MAIN wpisuje upscaled 2K)
│   ├── tentacles/
│   └── enemies/
├── 4k/
│   ├── weapons/     (tu MAIN wpisuje upscaled 4K)
│   ├── tentacles/
│   └── enemies/
├── 8k/
│   ├── weapons/     (tu MAIN wpisuje upscaled 8K)
│   ├── tentacles/
│   └── enemies/
└── upscale_manifest.csv   (lista plików do batch upscalingu)
```

### Krok C — Upscaling AI (MAIN, na swoim komputerze)

Uruchom wybrany AI upscaler na folderze `extracted_textures/2k/source/` (lub bezpośrednio na kategoriach) z parametrami:

| Cel | Rozmiar dłuższego boku | Model AI | Uwagi |
|-----|----------------------|----------|-------|
| 2K  | 2048 px              | Real-ESRGAN / ESRGAN | bazowy VR |
| 4K  | 4096 px              | Real-ESRGAN / ESRGAN | wysoka jakość |
| 8K  | 8192 px              | Real-ESRGAN / ESRGAN | maksymalna, duży VRAM |

Zasady upscalingu:
- Zachowaj oryginalny stosunek stron
- Dla każdej kategorii (weapons, tentacles, enemies) upscaluj osobno
- Zapisz wyniki do odpowiednich folderów `2k/`, `4k/`, `8k/` zgodnie z kategorią
- **Mapy normalne (`_n`)**: ostrożnie — upscaling AI może zniekształcić wektory normalnych; rozważ użycie oryginału lub delikatniejszego modelu
- **Mapy emissive (`_e`)**: upscaluj, ale zachowaj jasność

### Krok D — Konwersja na DDS (opcjonalne, zalecane dla D3D9)

Gra to x86 D3D9Ex. Tekstury końcowe najlepiej w formacie **DDS**:
- Kolor bez alfy: **DXT1**
- Kolor z alfą / normalne: **DXT5** lub **BC7**
- Z mipmapami (łańcuch 1×1 → rozmiar bazowy)
- Rozmiary potęgi dwójki (2048, 4096, 8192)

Narzędzia: GIMP + DDS wtyczka, Paint.NET, NVIDIA Texture Tools, Intel ISPC Texture Compressor.

### Krok E — Wdrożenie do gry (MAIN, Steam exclusive)

1. Zrób kopię zapasową oryginalnych pakietów tekstur (`Cache.Windows\B.Texture.toc/.cache` itp.)
2. Wdróż upscaled tekstury:
   - Opcja 1 (preferowana): jeśli `d3d9.dll` proxy wspiera loose-texture override — wrzuć pliki do folderu override (do zweryfikowania przez MAIN)
   - Opcja 2: podmień tekstury bezpośrednio w pakietach `toc`/`cache` (wymaga repackera — obecnie w repo jest tylko ekstraktor quickbms, nie repacker)
3. Uruchom grę przez Steam i przetestuj:
   - Czy bronie, macki, przeciwnicy wyświetlają się poprawnie w 2K/4K/8K
   - Czy nie ma crashy (szczególnie przy 8K — limit VRAM i rozmiaru tekstury D3D9)
   - Czy VR (SubmitMode 4) działa bez artefaktów

---

## 3. Otwarte ryzyka (do weryfikacji przez MAIN)

| Ryzyko | Opis |
|--------|------|
| Brak repackera | `darkness2.bms` tylko wyciąga, nie pakuje z powrotem. Wymuszenie tekstur może wymagać nowego kodu w `d3d9.dll` (hook ładowania tekstur) lub ręcznej edycji pakietów |
| Limit D3D9 | 8192×8192 może przekroczyć VRAM lub limity sterownika na starszych kartach |
| Mapy normalne | AI upscaling może zepsuć mapy normalne — traktuj je osobno lub używaj oryginałów |
| Brak loose-texture override w d3d9.dll | Obecny proxy nie ładuje zewnętrznych plików tekstur — MAIN musi dodać hook lub repakować |

---

## 4. Następne kroki

1. MAIN uruchamia skrypt extract+sort+prepare
2. MAIN instaluje wybrane AI i przeprowadza upscaling 2K/4K/8K
3. MAIN konwertuje na DDS (opcjonalnie) i wdraża do gry
4. MAIN testuje w grze (Steam exclusive) i raportuje wyniki
5. W razie potrzeby — nowa gałąź z kodem hooka tekstur w `d3d9.dll`
