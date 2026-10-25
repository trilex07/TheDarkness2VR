$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $env:TEMP 'd2vr-native-hands-math-test.exe'
$obj = Join-Path $env:TEMP 'd2vr-native-hands-math-test.obj'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 native_hands_math_test.cpp /Fo:`"$obj`" /Fe:`"$output`""
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    foreach ($seed in @('0xD2A11725','0x00000001','0x9E3779B9','0x243F6A88','0xB7E15162',
                       '0xDEADBEEF','0x13579BDF','0xC001D00D','0xF00DBABE','0xFFFFFFFF')) {
        & $output $seed
        if ($LASTEXITCODE -ne 0) { throw "geometry tests failed for seed $seed" }
    }
} finally { Pop-Location }
