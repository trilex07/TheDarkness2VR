@echo off
rem MAIN: build + host test + preflight + install of weapon wheel v3. Refuses if the game runs.
cd /d "%~dp0"
set LOG=%~dp0build\install-weapon-wheels-v3.log
if not exist build mkdir build
echo === %date% %time% === > "%LOG%"
powershell -NoProfile -ExecutionPolicy Bypass -File .\test\build_dual_weapon_wheel.ps1 >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -DiagnosticCapture >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\deploy-weapon-wheels-v3.ps1 >> "%LOG%" 2>&1 || goto fail
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\deploy-weapon-wheels-v3.ps1 -Install >> "%LOG%" 2>&1 || goto fail
echo RESULT: OK >> "%LOG%"
type "%LOG%"
timeout /t 8
exit /b 0
:fail
echo RESULT: FAILED >> "%LOG%"
type "%LOG%"
pause
exit /b 1
