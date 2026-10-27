$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$out=Join-Path $env:TEMP ('d2vr-frame-refs-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out|Out-Null
$source=Join-Path $here 'frame_resource_refs_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fe:frame_resource_refs_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'compile failed' }
    & .\frame_resource_refs_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'test failed' }
} finally { Pop-Location }
