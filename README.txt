Darkness2VR v0.1.8 (alpha)
========================

Steam VR build: D3D9 proxy + OpenXR for The Darkness II.
Mono cinema screen / projection + VR controller to XInput mapping.

VR settings (menu glowne oraz pauza)
----------------------------------
  Wybierz dodatkowy przycisk "VR settings" prawym spustem, celujac
  kontrolerem. Zakladki: Obraz, Jezyk oraz Sterowanie. Kolko zamyka panel;
  panel przejmuje wejscie do puszczenia przyciskow, aby nie wybrac opcji gry.
  INSERT otwiera ten sam panel z klawiatury.
  NIS: wlacz/wylacz oraz ostrosc 0-100%, zmiana od razu. Procent oznacza
  sile wyostrzania, nie skale rozdzielczosci ani zasieg obszaru spojrzenia.
  Supersampling: 100/125/150% kazdego wymiaru, po restarcie. Niedostepne
  poziomy sa pomijane; 125% to ok. 156% pikseli, 150% to 225%.
  Dubbing polski i tekst polski mozna wlaczac niezaleznie. Zmiana wymaga
  restartu; po zamknieciu gry lokalny pomocnik podmienia zweryfikowane
  archiwa jezyka. Przy ponownym starcie odczekaj zakonczenie tej operacji.
  Nie usuwaj folderu VRSettings: zawiera obie wersje i kopie transakcji.
  Ustawienia sa zapisywane w [VRSettings] aktywnego Darkness2VR.ini.

Cele misji (prawy kontroler VR)
------------------------------
  Przytrzymaj kolko (prawy gorny przycisk) przez 1 sekunde, aby pokazac
  tekst celu misji. Przestrzenny znacznik jest ukryty; prowadzi natywna
  fioletowa mgla. Krotkie nacisniecie wykonuje zwykla akcje kolka
  po puszczeniu. W menu kolko nadal dziala od razu jako Wstecz.

Celowanie lewej macki (chwyt)
----------------------------
  Gdy GazeGrab=1 i probka wzroku jest aktualna, cel wybiera spojrzenie.
  Bez poprawnego wzroku celuj lewym kontrolerem VR. Promien chwytu jest
  gotowy przed nacisnieciem chwytu; nie wymaga wycelowania glowa.
  Przy utracie obu zrodel mod wraca do natywnego zapytania gry.

Trojkat i wybory
---------------
  Pojedyncze nacisniecie trojkata od razu wysyla natywne Y, takze w rozmowie.
  Dwa krotkie nacisniecia (drugie do 350 ms po puszczeniu pierwszego)
  otwieraja pauze. Pierwsze nacisniecie nadal moze wykonac akcje w grze.
  Podczas rozpoznanego QTE, wyboru egzekucji lub decyzji fabularnej
  szybkie nacisniecia pozostaja natywne; pauza jest na przycisku menu.

VR / kino: awaryjne przelaczanie (CinematicTheater=1, SubmitMode=4)
--------------------------------------------------------------
  Przytrzymaj L3+R3 przez 1,5 sekundy: zwykly VR <-> kino 3D.
  Reczne ustawienie dziala przez 60 sekund, potem automatyka wraca sama.
  Kolejne przytrzymanie przelacza tryb i rozpoczyna nowa minute.
  Pusc oba przyciski przed kolejnym przelaczeniem. Krotkie nacisniecie
  nadal centruje widok i prostuje kat kamery gry w rozgrywce (takze
  na poczatku przytrzymania), zachowujac kierunek obrotu postaci.
  Uzyj tego, gdy kino wlaczy sie podczas zwyklego chodzenia lub rozmowy.
  Po restarcie gry od razu dziala automatyczne wykrywanie cutscenek/video.

Controls & Diagnostics (live hotkeys in game):
---------------------------------------------
  F9  : Cycle Test Pattern (0 = Real Game Capture [default], 1 = Solid Bright Green, 2 = Checkerboard)
  F10 : Cycle Submit Mode (0 = QUAD_LOCAL_HEAD [v0.1.3 cinema], 1 = QUAD_VIEW [view-fixed], 2 = PROJECTION_MONO [v0.1.2 stereo])
  F11 : Cycle Distance (1.5m -> 2.0m -> 2.5m -> 3.0m)

Configuration:
--------------
  %LOCALAPPDATA%\Darkness2VR\Darkness2VR.ini  (or Darkness2VR.ini beside DarknessII.exe)
  [Render]
  TestMode=0      ; 0 = Game capture, 1 = Solid green, 2 = Checkerboard
  SubmitMode=0    ; 0 = QUAD_LOCAL_HEAD (cinema), 1 = QUAD_VIEW, 2 = PROJECTION_MONO
  DistanceM=2.0
  WidthM=2.1
  [Input]
  EnableVRGamepad=1
  RemapPhysicalPorts=1

Install:
--------
1. Make sure SteamVR / PSVR2 App is running.
2. From this folder run:
     powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install
3. Launch "The Darkness II" from Steam normally.

Log:
----
  %LOCALAPPDATA%\Darkness2VR\d3d9.log

Mission guidance: hold the upper button on the right controller (Circle) for
one second to show the objective text. The screen-space destination marker is
hidden in the local HUD asset patch; follow the native purple world guidance.

Experimental performance candidate (2026-11-03):
Render/ShaderDemandCache=1 enables indexed, demand-prioritized shader learning
in a separate shader-demand-indexed.bin archive (4096 codes, 4 MiB; startup
inventory remains 2048 one-use objects). Render/ReplayPrepareWorker=1 enables
an experimental same-frame numeric stereo worker. Both default to 0 in the
repository. The prepared test package defaults to Cache only: the worker has
not passed the performance-benefit gate. Neither setting changes resolution.
Use the frozen package Install.ps1 with the game closed; -Mode Cache, Worker,
Both or Off selects an independent profile, and -Restore restores the exact
previous DLL and INI. Restart the game between modes. FPS gains are unverified.
SMAA trial (2026-11-04): [Render] SMAA=1 enables SMAA 1x HIGH after NIS.
Default OFF. It filters detected edges at the existing eye resolution, with no
temporal history/jitter or guessed depth/motion. It may soften thin details;
it does not guarantee removal of temporal shimmer. FXAA can stay disabled.
Diagnostics/SmaaExperiment=1 enables the same path for the isolated candidate
without changing the accepted main INI. Restart after changing either setting.
Both current eyes fall back together if filtering is unavailable. Cached D3D11
commands retain independent current inputs; no CPU image readback or per-frame
command recording. Do not combine this trial with the rejected DLAA experiment.
NIS tracking fallback: Render/FoveaCenterFallback=1 (default) keeps the
sharpening area at the fixed image center when an eye gaze is unavailable.
This prevents switching to full-image NisSharpness during blinks. Value0
restores the legacy uniform fallback. Resolution and NIS strength are unchanged.
