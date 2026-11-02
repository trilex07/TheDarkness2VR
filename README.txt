Darkness2VR v0.1.8 (alpha)
========================

Steam VR build: D3D9 proxy + OpenXR for The Darkness II.
Mono cinema screen / projection + VR controller to XInput mapping.

Cele misji (prawy kontroler VR)
------------------------------
  Przytrzymaj kolko (prawy gorny przycisk) przez 1 sekunde, aby pokazac
  cel misji i znaczniki. Krotkie nacisniecie wykonuje zwykla akcje kolka
  po puszczeniu. W menu kolko nadal dziala od razu jako Wstecz.

VR / kino: awaryjne przelaczanie (CinematicTheater=1, SubmitMode=4)
--------------------------------------------------------------
  Przytrzymaj L3+R3 przez 1,5 sekundy: zwykly VR <-> kino 3D.
  Reczne ustawienie dziala przez 60 sekund, potem automatyka wraca sama.
  Kolejne przytrzymanie przelacza tryb i rozpoczyna nowa minute.
  Pusc oba przyciski przed kolejnym przelaczeniem. Krotkie nacisniecie
  nadal centruje widok (takze na poczatku przytrzymania).
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
