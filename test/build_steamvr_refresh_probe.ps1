param([switch]$Run)
$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'steamvr_refresh_probe.cpp'
$exe=Join-Path $env:TEMP 'd2vr-steamvr-refresh-probe.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-steamvr-refresh-probe.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'SteamVR refresh probe compile failed'}
if($Run) {
    if(!@(Get-Process -Name vrserver -ErrorAction SilentlyContinue).Count){throw 'SteamVR is not already running; do not start it'}
    $games=@(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^(DarknessII|ChernobylGame.*|Singularity)\.exe$' -or
        ($_.ExecutablePath -like '*\steamapps\common\*' -and
         $_.ExecutablePath -notmatch '\\(SteamVR|Steamworks Shared|fpsVR)\\')
    })
    if($games.Count){throw 'A game is active; defer the optional read-only probe'}
    & $exe
    if($LASTEXITCODE -notin @(0,3)){throw 'Refresh probe failed'}
    exit $LASTEXITCODE
}
