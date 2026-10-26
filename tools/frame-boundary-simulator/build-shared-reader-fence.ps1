$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-shared-reader-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Push-Location $outDir
try {
    $source=Join-Path $repo 'test\shared_reader_fence_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fe:shared-reader-test.exe"
    if($LASTEXITCODE -ne 0){throw 'shared reader build failed'}
    & .\shared-reader-test.exe
    if($LASTEXITCODE -ne 0){throw 'shared reader tests failed'}
} finally {Pop-Location}
