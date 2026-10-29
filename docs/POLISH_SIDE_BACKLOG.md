# SIDE — dokończenie polonizacji

Lista pochodzi z faktycznie wgranego pakietu, nie z deklarowanej liczby
wygenerowanych WAV. Wszystkie 12 plików cache/TOC zostało porównanych SHA-256
z manifestem wdrożenia. Prywatne CSV, zawierające oryginalne teksty gry, są tutaj:

`%LOCALAPPDATA%\Darkness2VR\runtime\polish-side-backlog-20261029-final`

- `texts.csv`: 13 527 niepustych wpisów z kluczami, kategoriami, tekstem EN,
  tekstem faktycznie wdrożonym i proponowanym PL, jeśli istnieje.
- `audio.csv`: 7 453 niewgrane zamiany, dokładne ścieżki, klucze, dostępny tekst,
  powód odrzucenia, czas oryginału i istniejącego PL, ścieżka WAV, priorytet.
- `fonts.csv`: sześć fontów wymagających rzeczywistych polskich glifów.
- `summary.json`: zakres, liczniki i hashe sprawdzonego pakietu.

## Kolejność pracy

1. **Polskie fonty**: `SubtitlesFont`, `MenuFont`,
   `darknessheader_19.syn`, `_24.syn`, `_26.syn`, `_32.syn`. Obecne 18 aliasów
   Unicode używa zwykłych łacińskich glifów: ogonki nie są narysowane.
   Wymagane znaki: ĄąĆćĘęŁłŃńÓóŚśŹźŻż. Ustalić rzeczywiste kodowanie atlasu
   `B.Texture_en`, dodać glify, UV i metryki bez naruszania pozostałych języków.
   Nie traktować atlasu jako A8 na podstawie samego rozmiaru. Glify Ó/ó z innego
   języka używają innego atlasu; same rekordy UV nie są przenośne.
2. **Interfejs kampanii**: `Menu` — 470 niezmienionych niepustych wpisów,
   `Tutorial` — 116, `SPGame` — 262, `Weapons` — 247, `Talents` — 292,
   `Relics` — 133, `Challenges` — 150, `Conversation` — 203.
   Pozostałe nazwy własne, liczby i identyfikatory zatwierdzać ręcznie:
   „niezmienione” nie oznacza automatycznie „wymaga tłumaczenia”.
3. **Brakujące kwestie kampanii i walka**: w `audio.csv` najpierw priorytety
   `1_SP_STORY` i `2_SP_AMBIENT_OR_COMBAT_REVIEW`. Uzupełnić teksty `SPBarks`
   (7 267 niezmienionych wpisów) i sprawdzić 29 niezmienionych `SPStoryDialog`.
   W całym katalogu 7 353 zasoby nie mają dopasowanego PL WAV.
   Odsłuchać przed generowaniem: nie dubbingować oddechów i krzyków bez słów.
   Ścieżka w żywym TOC nie dowodzi użycia w obecnej kampanii; starsze/demo
   warianty mają oddzielny priorytet przeglądu.
4. **Naprawić istniejące nagrania zamiast ponownie generować**:
   97 `RETIME_EXISTING_PL_RECORDING` — dopasować wypowiedź do natywnego czasu,
   zachować ograniczenia strumienia resident i zsynchronizowanie sceny;
   3 `REPACK_SERIALIZATION_REQUIRED` — ustalić format serializacji.
   Te trzy zasoby to `Sd00220Swifty`, `Sd00350Wiseguy2`, `Sd06110Joey`;
   pełne ścieżki są w CSV. Nie zdejmować kontroli czasu ani ograniczeń pakowania.
5. **Vendettas/MP**: `MPBarks` — 1 151, `MPGame` — 431,
   `MPStoryDialog` — 400 niezmienionych wpisów oraz audio z priorytetu 3.
6. **QA istniejącego PL**: 2 375 zmienionych tekstów (103 menu + 2 272 fabuła)
   i 1 314 wgranych zamian audio wymagają kontroli językowej i odsłuchu.
   Zmiana bajtów albo poprawne dekodowanie nie dowodzi jakości tłumaczenia.

## Wymagania zwrotne dla MAIN

- Poprawiony manifest UTF-8: dokładny klucz gry, EN, PL, mówca, ścieżka WAV,
  oryginalny czas i stan QA. Zachować znaczniki i zmienne formatowania.
- Nagrania źródłowe poza Git; wykaz mapowania do rzeczywistych ścieżek TOC,
  oddzielnie brakujące, poprawiane i pomijane świadomie zasoby.
- Fonty: dokumentacja dekodowania/ponownego kodowania atlasu, walidacja UV,
  metryk i niezmienionych glifów; podgląd wszystkich polskich znaków.
- Pakiet prywatny, hashe, dekodowanie każdej zmiany audio i kontrola native
  packet alignment/dpds oraz czasów. Nie zastępować RIFF xWMA kontenerem ASF.
- SIDE nie nadpisuje Steam ani pracy MAIN. MAIN scala i wdraża transakcyjnie
  z kopią przywracania; bez EXE patchy i bez zmian SteamVR.

Generator: `tools/polish_side_backlog.py`. Wymaga stagingu naprawionego pakietu,
aktualnego `Cache.Windows`, źródłowego manifestu i katalogu WAV. Odmawia eksportu,
gdy hashe wdrożenia lub liczniki dopasowanych/wgranych nagrań się nie zgadzają.
Oryginalne teksty, WAV i wyeksportowane CSV nie trafiają do repozytorium.
