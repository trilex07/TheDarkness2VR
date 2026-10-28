$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$out=Join-Path $env:TEMP 'd2vr-native-aim-getter'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$fixture=Join-Path $PSScriptRoot 'native_aim_getter_fixture.cpp'
$test=Join-Path $PSScriptRoot 'native_aim_getter_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /LD /EHsc /W4 /O2 /MD /std:c++17 `"$fixture`" /Fe:fixture.dll /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'Getter fixture build failed'}
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$test`" /Fe:test.exe"
    if($LASTEXITCODE -ne 0){throw 'Getter test build failed'}
    & .\test.exe (Join-Path $out 'fixture.dll')
    if($LASTEXITCODE -ne 0){throw 'Getter ABI regression failed'}
} finally {Pop-Location}
