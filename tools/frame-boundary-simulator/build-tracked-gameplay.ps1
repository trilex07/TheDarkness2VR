param([string]$Exe = '', [switch]$CameraCover)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$name = if($CameraCover) { 'native_camera_cover_test' } else { 'tracked_gameplay_gate_test' }
$output = Join-Path $env:TEMP "d2vr-$name.exe"
$obj = Join-Path $env:TEMP "d2vr-$name.obj"
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 $name.cpp /Fo:`"$obj`" /Fe:`"$output`""
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    if ($Exe) { & $output $Exe } else { & $output }
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
} finally { Pop-Location }
