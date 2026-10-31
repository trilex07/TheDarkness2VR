$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$out=Join-Path $env:TEMP 'd2vr-native-hud'
New-Item -ItemType Directory -Force -Path $out|Out-Null
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$test=Join-Path $PSScriptRoot 'native_hud_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$out`" `"$test`" /Fe:test.exe"
    if($LASTEXITCODE -ne 0){throw 'Native HUD build failed'}
    & .\test.exe
    if($LASTEXITCODE -ne 0){throw 'Native HUD regression failed'}
} finally {Pop-Location}
