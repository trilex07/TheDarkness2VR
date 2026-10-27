param([switch]$Hardware)
$ErrorActionPreference='Stop'
$out=Join-Path $env:TEMP 'd2vr-query-preload-tests'
New-Item -ItemType Directory -Path $out -Force|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'query_preload_pool_test.cpp'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fe:query_preload_pool_test.exe /link user32.lib"
 if($LASTEXITCODE -ne 0){throw 'Query fixture build failed'}
 if($Hardware){& .\query_preload_pool_test.exe --hardware}else{& .\query_preload_pool_test.exe}
 if($LASTEXITCODE -ne 0){throw 'Query fixture failed'}
} finally {Pop-Location}
