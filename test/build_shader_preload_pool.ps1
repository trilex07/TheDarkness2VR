param([switch]$Hardware)
$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$output=Join-Path $env:TEMP 'd2vr-shader-preload-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $here 'shader_preload_pool_test.cpp'
Push-Location $output
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fe:shader_preload_pool_test.exe /link user32.lib shell32.lib d3dcompiler.lib"
 if($LASTEXITCODE -ne 0){throw 'compile failed'}
 if($Hardware){& .\shader_preload_pool_test.exe --hardware}else{& .\shader_preload_pool_test.exe}
 if($LASTEXITCODE -ne 0){throw 'shader preload regression failed'}
} finally {Pop-Location}
