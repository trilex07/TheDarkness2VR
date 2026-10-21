$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -property installationPath
$vc = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $here
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 stereo_harness.cpp /Fe:stereo_harness.exe"
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    .\stereo_harness.exe
} finally { Pop-Location }
