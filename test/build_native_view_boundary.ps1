$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-native-view-boundary'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$fixture=Join-Path $PSScriptRoot 'native_view_boundary_fixture.cpp'
$test=Join-Path $PSScriptRoot 'native_view_boundary_test.cpp'
$mh=Join-Path $repo 'third_party\minhook\src'
Push-Location $out
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /LD /EHsc /W4 /Od /MD /std:c++17 `"$fixture`" /Fe:fixture.dll /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'Boundary fixture build failed'}
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$test`" `"$mh\buffer.c`" `"$mh\hook.c`" `"$mh\trampoline.c`" `"$mh\hde\hde32.c`" /Fe:test.exe"
    if($LASTEXITCODE -ne 0){throw 'Boundary test build failed'}
    & .\test.exe (Join-Path $out 'fixture.dll')
    if($LASTEXITCODE -ne 0){throw 'Boundary ABI regression failed'}
} finally {Pop-Location}
