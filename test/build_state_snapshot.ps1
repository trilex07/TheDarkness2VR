param([switch]$Hardware)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-state-snapshot-tests'
New-Item -ItemType Directory -Path $out -Force|Out-Null
$text=Get-Content -LiteralPath "$repo\src\stereo_replay.inl" -Raw
$body=[regex]::Match($text,'(?s)struct DeviceSnapshot \{.*?\r?\n\};')
if(-not $body.Success){throw 'Actual snapshot source missing'}
$body.Value|Set-Content -LiteralPath "$out\actual_snapshot.inl" -Encoding utf8
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'state_snapshot_test.cpp'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /std:c++17 /I. `"$source`" /Fe:state_snapshot_test.exe /link user32.lib"
 if($LASTEXITCODE -ne 0){throw 'Snapshot test compile failed'}
 if($Hardware){& .\state_snapshot_test.exe --hardware}else{& .\state_snapshot_test.exe}
 if($LASTEXITCODE -ne 0){throw 'Snapshot test failed'}
} finally {Pop-Location}
