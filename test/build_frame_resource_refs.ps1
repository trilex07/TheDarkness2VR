$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /std:c++17 frame_resource_refs_test.cpp /Fe:frame_resource_refs_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'compile failed' }
    & .\frame_resource_refs_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'test failed' }
} finally { Pop-Location }
