Darkness2VR v0.1.8 (alpha)
========================

Steam VR build: D3D9 proxy + OpenXR for The Darkness II.
Mono cinema screen / projection + VR controller to XInput mapping.

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
