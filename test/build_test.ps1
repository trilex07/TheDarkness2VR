# Builds and runs the stereo geometry test harness (x86, real D3D9 device).
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"

# Build outside the repo into a fresh folder: a stale exe in test\ can stay
# delete-pending (LNK1104), and binaries must not land in the working tree.
$out = Join-Path $env:TEMP ("d2vr-stereo-test-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $out 'stereo_test.exe'
Push-Location $here
try {
    $cmd = "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 stereo_test.cpp /Fo:`"$out\stereo_test.obj`" /Fe:`"$exe`" /link d3d9.lib d3dcompiler.lib user32.lib"
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "test build failed" }
    Write-Host "--- running stereo_test.exe ---"
    & $exe
    $code = $LASTEXITCODE
    Write-Host "--- exit: $code ---"
    if ($code -ne 0) { throw "stereo test failed (exit $code)" }
} finally {
    Pop-Location
}