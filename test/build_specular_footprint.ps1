param([Parameter(Mandatory=$true)][string]$NativeShader)
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$shader=(Resolve-Path -LiteralPath $NativeShader).Path
$output=Join-Path $env:TEMP 'd2vr-specular-footprint'
New-Item -ItemType Directory -Force $output|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $output
try {
    $source=Join-Path $here 'specular_footprint_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /std:c++17 `"$source`" /Fe:specular_footprint_test.exe /link d3dcompiler.lib user32.lib"
    if($LASTEXITCODE -ne 0){throw 'Specular footprint compile failed'}
    & .\specular_footprint_test.exe $shader
    if($LASTEXITCODE -ne 0){throw 'Specular footprint regression failed'}
}finally{Pop-Location}
