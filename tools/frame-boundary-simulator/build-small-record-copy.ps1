$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-small-record-copy-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Push-Location $outDir
try {
    $test=Join-Path $here 'small_record_copy_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$test`" /Fe:small-record-copy-test.exe"
    if($LASTEXITCODE -ne 0){throw 'small record copy test build failed'}
    & .\small-record-copy-test.exe (Join-Path $repo 'Darkness2VR.ini')
    if($LASTEXITCODE -ne 0){throw 'small record copy tests failed'}
} finally {Pop-Location}
