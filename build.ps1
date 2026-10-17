# Builds Darkness2VR d3d9.dll (x86) for The Darkness II.
param(
    [switch]$Install,
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Darkness II"
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

$sdkRoot = $env:OPENXR_SDK
if (-not $sdkRoot) { $sdkRoot = "C:\Users\Trilex\openxr-sdk" }
$inc = Join-Path $sdkRoot "include"
$libDir = Join-Path $sdkRoot "native\Win32\release\lib"
$binDir = Join-Path $sdkRoot "native\Win32\release\bin"
if (-not (Test-Path (Join-Path $libDir "openxr_loader.lib"))) {
    $libDir = Join-Path $sdkRoot "Win32\lib"
    $binDir = Join-Path $sdkRoot "Win32\bin"
}
foreach ($p in @($inc, $libDir, $binDir)) {
    if (-not (Test-Path $p)) { throw "OpenXR SDK path missing: $p (set OPENXR_SDK)" }
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"

Push-Location $here
try {
    $cmd = "`"$vcvars`" x86 && cl /nologo /LD /EHsc /W3 /O2 /MD /std:c++17 " +
           "/I`"$inc`" /Fe:d3d9.dll src\d3d9.cpp " +
           "/link /DEF:d3d9.def /LIBPATH:`"$libDir`" openxr_loader.lib dxgi.lib d3d11.lib user32.lib shell32.lib xinput.lib"
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
    Write-Host "Built: $(Join-Path $here 'd3d9.dll')"

    if ($Install) {
        if (-not (Test-Path -LiteralPath $GameDir)) { throw "Game dir not found: $GameDir" }
        $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $target = Join-Path $GameDir "d3d9.dll"
        if (Test-Path $target) {
            $bak = Join-Path $GameDir "d3d9.dll.off-$stamp"
            Copy-Item $target $bak -Force
            Write-Host "Backup: $bak"
        }
        Copy-Item (Join-Path $here "d3d9.dll") $target -Force
        Copy-Item (Join-Path $binDir "openxr_loader.dll") $GameDir -Force
        Write-Host "Installed to: $GameDir"
    }
} finally {
    Pop-Location
}
