# MAIN: build protected-native candidate, run host test, preflight, deploy (DLL only). Output -> main-run.log
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$log=Join-Path $here 'main-run.log'
Start-Transcript -LiteralPath $log -Force | Out-Null
try {
  Set-Location $here
  Write-Host "== BUILD (protected-native: -DiagnosticCapture)"
  & "$here\build.ps1" -DiagnosticCapture
  Write-Host "== DLL SHA256: $((Get-FileHash "$here\d3d9.dll" -Algorithm SHA256).Hash)"
  Write-Host "== HOST TEST"
  & "$here\test\build_unarmed_trigger_dpad.ps1"
  Write-Host "== PREFLIGHT"
  & "$here\tools\deploy-unarmed-trigger-dpad.ps1"
  Write-Host "== INSTALL"
  & "$here\tools\deploy-unarmed-trigger-dpad.ps1" -Install
  Write-Host "MAIN-RUN RESULT: SUCCESS"
} catch {
  Write-Host "MAIN-RUN RESULT: FAILED: $($_.Exception.Message)"
  Write-Host $_.ScriptStackTrace
} finally { Stop-Transcript | Out-Null }
