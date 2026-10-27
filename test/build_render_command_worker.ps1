param([switch]$D3D9Reference)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$outputDir = Join-Path $env:TEMP 'd2vr-render-command-worker-tests'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$testName = if ($D3D9Reference) { 'render_worker_d3d9_test' } else { 'render_command_worker_test' }
$source = Join-Path $here "$testName.cpp"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $outputDir
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fe:$testName.exe /link user32.lib"
    if ($LASTEXITCODE -ne 0) { throw 'compile failed' }
    & (Join-Path $outputDir "$testName.exe")
    if ($LASTEXITCODE -ne 0) { throw 'test failed' }
} finally { Pop-Location }
