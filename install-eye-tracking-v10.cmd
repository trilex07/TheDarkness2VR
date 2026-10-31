@echo off
rem MAIN: build + host test + preflight + install of eye tracking v10. Refuses if the game runs.
cd /d "%~dp0"
set LOG=%~dp0build\install-eye-tracking-v10.log
if not exist build mkdir build
echo === %date% %time% === > "%LOG%"
powershell -NoProfile -ExecutionPolicy Bypass -File .\test\build_dual_weapon_wheel.ps1 >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\test\build_eye_tracking.ps1 >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -DiagnosticCapture >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\psvr2\build-bridge.ps1 -OutputDirectory "%~dp0build\psvr2-bridge" >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\deploy-eye-tracking-v10.ps1 >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\deploy-eye-tracking-v10.ps1 -Install >> "%LOG%" 2>&1 || goto fail
echo RESULT: OK >> "%LOG%"
type "%LOG%"
timeout /t 8
exit /b 0
:fail
echo RESULT: FAILED >> "%LOG%"
type "%LOG%"
pause
exit /b 1
