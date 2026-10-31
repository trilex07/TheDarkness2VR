# High-Res Texture Extraction & Preparation Plan — The Darkness II (2K / 4K / 8K)

**Branch**: `side/texture-hires-extraction-2k4k8k`
**Role**: SIDE (offline only — no Steam launch, no game-dir modification, no runtime test)
**Owner**: MAIN performs extraction, upscaling, and installation in the game dir.
**Focus**: weapons, tentacles (macki), enemies (przeciwnicy).

---

## 1. Jak działa pakietowanie tekstur w grze

The Darkness II (Evolution Engine, Steam AppID 67370) przechowuje zasoby w pakietach:
- `toc` — spis plików (offsety, rozmiary, nazwy)
- `cache` — dane skompresowane (chunked, kompresja `evolution` lub `oodle`)

W repo znajduje się gotowy skrypt QuickBMS: `tools/darkness2.bms` (script 0.2.3, od GMMan's Evolution Engine Extractor).

Przykładowe użycie (MAIN, na własnej kopii plików gry):
```
quickbms.exe tools/darkness2.bms <folder-gry>\toc <wyjscie>
```
Wymaga plików `toc` i `cache` z katalogu gry (Steam: `C:\Program Files (x86)\Steam\steamapps\common\Darkness II\`).

Skrypt wydobędzie pliki z zachowaniem struktury katalogów i nazw. Tekstury mogą być w formacie DDS lub jako surowe dane obrazu z nagłówkiem własnym silnika — trzeba je zidentyfikować po rozszerzeniu/nazwie lub podglądzie bajtów.

---

## 2. Co extractować (priorytety)

Wg użytkownika, główne cele:
1. **Bronie** (weapons) — pistolety, strzelby, broń ciemności
2. **Macki** (tentacles) — ciemne węże/szpony z uniwersum The Darkness
3. **Przeciwnicy** (enemies) — modele i tekstury wrogów

Metoda identyfikacji plików tekstur (MAIN, po extractcie):
- Poszukać plików z rozszerzeniem `.dds`, `.txr`, `.tex`, `.texture` lub bez rozszerzenia
- Poszukiwać po nazwach zawierających: `weapon`, `gun`, `pistol`, `shotgun`, `tentacle`, `tentakl`, `enemy`, `mob`, `demon`, `darkling`
- Podglądnąć w narzędziu do DDS (np. GIMP z wtyczką DDS, Paint.NET, Noesis, TexMod) aby potwierdzić że to tekstura a nie skrypt/model

---

## 3. Upscaling do 2K / 4K / 8K

Dla każdej wybranej tekstury źródłowej przygotować trzy wersje:
- **2K** — dłuższy bok 2048 px
- **4K** — dłuższy bok 4096 px
- **8K** — dłuższy bok 8192 px

Narzędzia (MAIN wybiera, SIDE nie testuje):
- **Real-ESRGAN** lub **ESRGAN** — AI upscaling, najlepsza jakość dla tekstur fotograficznych i malarskich
- **waifu2x** — alternatywa dla tekstur z wyraźnymi krawędziami (komiksowe/stylizowane)
- **Photoshop / GIMP** — interpolacja Lanczos lub bicubic jako opcja awaryjna

Zasady:
- Zachować oryginalny stosunek stron
- Dla map normalnych/roughness/specular (jeśli są osobne pliki) upscalować osobno lub korzystać z oryginału (nie wymuszać AI na mapach normalnych — może zniekształcić wektory normalnych)
- Zapis bez stratnej kompresji w intermediate (PNG lossless), a końcowo jako DDS

---

## 4. Format końcowy dla D3D9 / Evolution Engine

Gra to x86 D3D9Ex. Tekstury końcowe powinny być:
- **Format**: DDS z kompresją **BC7** (najlepsza jakość, D3D9Ex obsługuje via driver) lub **DXT5** dla map z alphą, **DXT1** dla koloru bez alfy
- **Mipmapy**: tak (wygenerować łańcuch mipmap 1x1 do rozmiaru bazowego)
- **Rozmiary**: potęgi dwójki (2048, 4096, 8192) — D3D9 lepiej obsługuje potęgi dwójki
- **Nazewnictwo**: takie samo jak oryginalny plik z pakietu, np. `weapon_pistol.dds`, `tentacle_01.dds`, `enemy_darkling.dds`

Struktura folderów do przekazania MAIN:
```
hires_textures/
  2k/
    weapons/
    tentacles/
    enemies/
  4k/
    ...
  8k/
    ...
```

---

## 5. Jak MAIN ma wgrać (tylko MAIN)

1. Zrobić kopię zapasową oryginalnych plików tekstur z pakietu gry (lub całego folderu gry)
2. Wyczyszczenie/wymiana plików w pakietach `toc`/`cache` lub podmiana w folderze override (jeśli gra/Proxy wspiera loose textures — do zweryfikowania przez MAIN)
3. Uruchomić grę przez Steam (MAIN tylko) i sprawdzić:
   - Czy tekstury 2K/4K/8K się ładują
   - Czy nie ma crashy przy 8K (pamięć wideo, D3D9 limit rozmiaru tekstury)
   - Czy bronie, macki i przeciwnicy wyglądają poprawnie
4. W razie problemu cofnąć kopię zapasową

**Uwaga SIDE**: Nie wiemy czy `d3d9.dll` proxy wspiera ładowanie zewnętrznych tekstur z folderu (loose texture override). Jeśli nie, MAIN musi podmieniać tekstury bezpośrednio w pakietach `toc`/`cache` (wymaga repackowania — nie ma w repo narzędzia do repacku, tylko extractu). To ryzyko do weryfikacji przez MAIN.

---

## 6. Otwarte pytania / ryzyka

| Ryzyko | Opis |
|--------|------|
| Brak repackera | `darkness2.bms` tylko wyciąga, nie pakuje z powrotem. Wymuszenie tekstur może wymagać edycji pakietów lub nowego kodu w `d3d9.dll` (hook texture loading) |
| Limit D3D9 tekstury | D3D9 może nie obsłużyć 8192x8192 na starszych kartach (max 16384 zazwyczaj, ale zależy od VRAM) |
| Mapy normalne | Upscaling AI może zepsuć mapy normalne — trzeba je traktować osobno |
| Brak źródeł | SIDE nie ma dostępu do plików gry (Steam, MAIN tylko), więc nie można przetestować extractu offline |

---

## 7. Następne kroki (MAIN)

1. Zainstalować quickbms i uruchomić `tools/darkness2.bms` na plikach gry
2. Wykazać tekstury bronia/macki/przeciwników wśród wyekstrahowanych plików
3. Przeprowadzić upscaling 2K/4K/8K wybranych tekstur
4. Przygotować foldery `hires_textures/{2k,4k,8k}/{weapons,tentacles,enemies}`
5. Sprawdzić czy `d3d9.dll` wspiera ładowanie zewnętrznych tekstur; jeśli nie — przygotować plan repack/pakietów
6. Wgrać do gry i przetestować (MAIN, Steam exclusive)
