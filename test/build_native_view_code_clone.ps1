$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'native_view_code_clone_test.cpp'
$hde=Join-Path $PSScriptRoot '..\third_party\minhook\src\hde\hde32.c'
$exe=Join-Path $env:TEMP 'd2vr-native-view-code-clone.exe'
Push-Location $env:TEMP
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" `"$hde`" /Fe:`"$exe`" /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'Owned-code fixture build failed'}
    $global:LASTEXITCODE=$null
    & $exe
    if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'Owned-code fixture failed or did not start'}
} finally {Pop-Location}
