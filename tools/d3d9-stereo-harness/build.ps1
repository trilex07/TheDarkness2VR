param([switch]$ConstantOwnership)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -property installationPath
$vc = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$out = if ($ConstantOwnership) { Join-Path $env:TEMP 'd2vr-constant-ownership-test' } else { $here }
New-Item -ItemType Directory -Path $out -Force | Out-Null
$source = if ($ConstantOwnership) { 'constant_ownership_test.cpp' } else { 'stereo_harness.cpp' }
$target = if ($ConstantOwnership) { 'constant_ownership_test.exe' } else { 'stereo_harness.exe' }
Push-Location $out
try {
    $sourcePath = Join-Path $here $source
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 `"$sourcePath`" /Fe:$target /link user32.lib"
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    & (Join-Path $out $target)
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
} finally { Pop-Location }
