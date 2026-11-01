param([switch]$Cost)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

& "$PSScriptRoot\build-dlss-host.ps1"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvarsall.bat"

$outDir = Join-Path $repo "build\dlss_host"
$testExe = Join-Path $outDir "dlss-host-tests.exe"
$cmd = "`"$vcvars`" x86 >nul && cl /nologo /O2 /EHsc /std:c++17 /W3 /MD " +
       "/I`"$repo\src`" " +
       "`"$PSScriptRoot\dlss-host-tests.cpp`" `"$repo\src\dlss\dlss_client.cpp`" `"$repo\src\dlss\dlss_gpu.cpp`" " +
       "/Fe:`"$testExe`" /Fo:`"$outDir\\`" /link d3d11.lib dxgi.lib d3dcompiler.lib"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "dlss-host-tests build failed ($LASTEXITCODE)" }

$hostExe = Join-Path $outDir "d2vr_dlss_host64.exe"
$extra = if ($Cost) { @("--cost") } else { @() }
& $testExe $hostExe $outDir @extra
exit $LASTEXITCODE
