# The Darkness II VR — plan napraw po instalatorze 1.1.1

## Primary goal

Gracz uruchamia grę w goglach, trafia laserem w widoczny przycisk i zatwierdza go jednym naciśnięciem zarówno w menu głównym, jak i w pauzie. Po powrocie do gry sterowanie i obraz w obu oczach nadal działają poprawnie.

## Faza 0 — ustalenie punktu startowego

> **Jak to służy celowi:** testy muszą dotyczyć dokładnie tego wydania, które dostał gracz, a wyniki starszych prób wskazują, co warto ponownie wykorzystać.

- Publiczny punkt startu: Darkness DLL z `331d5b41`, SHA-256 `6F913407289093107388109F624B7BCAF1030189D46988F4B16334E0021C5D27`, katalog 1.1.8. To osobna linia od wcześniejszych buildów testowanych w goglach. Zapisz SHA DLL, EXE i obu INI, aktywny runtime OpenXR, model gogli i scenę. Porównaj zaakceptowany profil MAIN z `BASELINE.md`. Przed próbą sprawdź kopię do wycofania i zapis gry.
- **Dozwolone sprawdzone punkty integracji:** istniejący obserwator Present i `InitXR` (raport [publicznego audytu](HANDOFF-darkness-public-audit-20261114.md)); zweryfikowane współrzędne Flash i natywna obsługa potwierdzenia z gałęzi menu; warstwa OpenXR dla obrazu lasera z powierzchni potwierdzonej w danej klatce. Każda funkcja ma mieć osobną próbę i prosty powrót.
- **Dowody historyczne:** [menu główne B3F](https://github.com/trilex07/TheDarkness2VR/blob/cabe8815/docs/agent-workflow/handoffs/HANDOFF-b3f-menu-pass-world-route-20261110.md) — zaznaczenie OPTIONS zmieniło kolor w obu oczach, koniec lasera pokrył się z celem; próba nie potwierdziła kliknięcia, Continue ani podmenu. [Relacja z gogli E6B8](https://github.com/trilex07/TheDarkness2VR/blob/0dbfbdbe/docs/agent-workflow/handoffs/HANDOFF-headset-followups-20261110.md) — laser w menu głównym działał, w pauzie był niewidoczny, laser broni wskazywał niżej niż trafienia. Nie przenoś tych wyników na instalator 1.1.1.
- **Weryfikacja:** przed pierwszą zmianą zapisz dla każdej funkcji „działa / odtworzono błąd / nie sprawdzono” wraz z identyfikatorem buildu i sceną. SIDE: testy offline. MAIN: rzeczywiste Steam i gogle, po zweryfikowaniu kopii. Zachowaj polski jako wybór, mimo że osobny stary kandydat miał menu zawsze po angielsku.

## Faza 1 — P0: start w goglach

> **Jak to służy celowi:** bez działającej sesji VR nie można wiarygodnie sprawdzić menu ani wejścia do świata.

- Poproś zgłaszającego o jeden start na 1.1.1: czy po ewentualnym wyborze języka doszedł do faktycznego menu 3D? Odczytaj `XRStartup` i sąsiednie linie z `d3d9.log` po zamknięciu gry. Użyj `src/image_present_runtime.inl` i `src/d3d9.cpp` z `331d5b41`.
- Brak `readyPresent`: sprawdź właściciela i stan Present/Reset. `readyPresent` bez `initAttempt`: sprawdź decyzję i bieżące bramki. Nieudane wywołanie XR: popraw tylko pierwszy wykazany błąd. Przy udanym `xrEndFrame` sprawdź osobno, czy wysłano obraz gry i czy gracz zobaczył go w goglach.
- Jeżeli potrzeba kolejnych zgłoszeń, dodaj opcjonalny lokalny eksport ograniczony do istotnych linii startu, ustawień i hashy, z usunięciem ścieżek użytkownika i bez automatycznej wysyłki.
- **Weryfikacja:** test offline wybranej gałęzi błędu; MAIN: menu i świat w obu oczach na znanym profilu; osobno to samo u zgłaszającego. Nie zmieniaj w ciemno `StartupMode`, aktywnego runtime ani reguł dopuszczania EXE.

## Faza 2 — P1: laser i kliknięcie w menu głównym

> **Jak to służy celowi:** gracz może wskazać właściwy element i przejść do gry bez sięgania po mysz.

- Jeśli brakuje ścieżki w publicznym DLL, przenieś wąski wariant `fab0a09d`, nie cały build B3F. W `src/native_flash_cursor.inl` użyj istniejących `FlashRayPoint` i `NativeFlashCursorReady`; `ControllerMenuConfirmReady` w `src/eye_gaze.inl` wybiera bieżącą ścieżkę. Natywne przeliczenie współrzędnych musi nadal należeć do aktywnego menu. `ApplyMenuTriggerConfirm` w `src/weapon_wheel_input.inl` emituje pojedyncze naciśnięcie na natywnej ścieżce A i zwalnia je; zwykłe A na padzie zachowuje działanie.
- Nie klikaj na braku trafienia, starej pozycji, utracie śledzenia albo fokusu. Nie przesuwaj systemowego kursora. Najpierw porównaj wskazanie w obu oczach, potem kliknięcie, potem wejście do świata.
- **Weryfikacja:** `build_native_flash_cursor.ps1`, `build_controller_menu_pointer.ps1`, `build_native_menu_mouse_poll.ps1`; w goglach: OPTIONS, podmenu, Wstecz, Continue, pojedyncze R2 bez powtórek przy przytrzymaniu, pad i odzyskanie sterowania po wyjściu z menu.

## Faza 3 — P1: laser i sterowanie w pauzie

> **Jak to służy celowi:** po zatrzymaniu rozgrywki laser jest widoczny i wybiera przycisk, który użytkownik naprawdę wskazał.

- **3A: widoczność.** Wąski kandydat `75e245ca` w `src/controller_menu_laser.inl` używa `RecordProjectedMenuSurface` i `AppendControllerMenuLaser` dla potwierdzonej powierzchni pauzy. Próbuj osobno, z `PauseMenuLaser=0` domyślnie do akceptacji. Sprawdź aktualną klatkę, przestrzeń, fokus i limit warstw. Brak lasera po wznowieniu gry.
- **3B: obsługa.** Z [dowodu współrzędnych pauzy](https://github.com/trilex07/TheDarkness2VR/blob/63fe7f70/docs/agent-workflow/handoffs/HANDOFF-pause-coordinate-proof-20261110.md) wykorzystaj powiązanie obszaru sceny i macierzy faktycznie rysowanego menu z wejściem tej samej aktywnej instancji. Najpierw zbierz jedną pełną próbkę rysowania i obsługi wejścia z tej samej instancji. Gdy ich nie da się powiązać, zatrzymaj się na samym wskaźniku wizualnym. Nie kopiuj przeliczenia z menu głównego i nie oznaczaj innej powierzchni jako tekstury Flash.
- **Weryfikacja:** `test/build_controller_menu_laser.ps1` i próby braku aktywnego menu/fokusu; gogle: Resume, ustawienia pauzy, Wstecz, ponowne otwarcie. Następnie strzał w świecie: nie może zostać przechwycony przez menu.

## Faza 4 — P2: celowanie, dryf i drabina

> **Jak to służy celowi:** po wyjściu z menu gracz może normalnie celować, patrzeć i przejść dalej.

- **Laser broni:** odtwórz rozjazd w stałej pozie; użyj opcjonalnego `ShotRenderWitness` [z gałęzi diagnostycznej](https://github.com/trilex07/TheDarkness2VR/blob/bb90f327/docs/agent-workflow/handoffs/HANDOFF-shot-render-witness-20261110.md) dla jednego strzału. Porównaj kierunek po rozrzucie, kamerę, widoczny wskaźnik obu oczu i trafienie z tej samej klatki. Popraw udowodnione przeliczenie; nie przesuwaj lasera w pionie na oko i nie zmieniaj kolizji ani rozrzutu. `test/build_shot_render_witness.ps1`; bliski i dalszy cel, oba oczy. Grubszy pierścień to osobna zmiana czytelności.
- **Dryf:** porównaj na tej samej DLL `UnifiedPadPackets=0/1`, w stałej pozie z ruchem pionowym i puszczeniem drążka; potwierdź kolizję numerów pakietów fizycznego i VR pada oraz wynik po wyzerowaniu. [Dowód pakietów](https://github.com/trilex07/TheDarkness2VR/blob/2b101b98/docs/agent-workflow/handoffs/HANDOFF-b3f-unified-pad-20261110.md); `test/build_horizon_input.ps1`. Nie maskuj dryfu recenterem ani blokadą pionu.
- **Drabina po Swiftym:** porównaj zwykły pad/klawiaturę z kontrolerami VR na tym samym zapisie. Sprawdź zdarzenie końca zejścia i rzeczywisty stan postaci, zanim zmienisz jej pozycję lub animację. Podwójne potrząśnięcie do przeładowania, chwyt i rzut prętem oraz zapamiętanie Cinema sprawdzaj później, po jednej funkcji.

## Faza 5 — P2: płynność i obraz po ustabilizowaniu sterowania

> **Jak to służy celowi:** interakcje i ruch pozostają płynne, a poprawki nie psują obrazu prawego oka.

- Użyj `docs/research/combat-scheduler-waits-20261104.md` i `tools/perf/analyze_scheduler_frames.py`. Zbierz porównywalną scenę w menu i w świecie, z obecnymi hashami. Porównaj medianę, p95 i p99 czasu klatki, faktyczną pracę CPU/GPU, czekanie sterownika i tempo wysyłania klatek do XR. Dopiero potem wybierz jedno zmierzone wąskie gardło.
- Jeżeli wróci usterka jednego oka, porównaj obie tekstury źródłowe i parę wysłaną do OpenXR z tej samej klatki. [Historia izolacji prawego oka](../../research/isolated-eye-output-20261103.md) i [oświetlenia](../../research/crane-smoke-projection-20261105.md). Nie wyłączaj synchronizacji GPU, nie nadpisuj globalnie stałych shaderów i nie przenoś historycznych wyników między buildami.
- **Weryfikacja:** identyczna scena przed/po, brak utraty klatek lub artefaktów stereo, test powrotu z pauzy i sceny walki. Bez nowego dowodu odłóż DLAA, generowanie klatek, globalne AF i duże zmiany renderera.

## Faza 6 — bramka następnej paczki

> **Jak to służy celowi:** instalator trafiający do graczy zawiera dokładnie to, co przeszło próby.

- Niezależnie od automatycznych testów potwierdź na Steamie w goglach: start, oba oczy, menu główne, pauzę, powrót do walki, pad i użycie broni. Sprawdź PSVR2 i konfigurację zgłaszającego osobno. Upewnij się, że polski nadal wymaga zaznaczenia opcji. Wycofaj kandydata przy regresji.
- Aktualizuj katalog po akceptacji odpowiedniego buildu, sprawdź podpis i SHA archiwów, instalację z paczki i pobranie przez aktualizację starszego instalatora. Nie pakuj kandydatów, które mają jedynie test offline.
