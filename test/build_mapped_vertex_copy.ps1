param([string]$SourceRef)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-mapped-vertex-copy'
New-Item -ItemType Directory -Force -Path $out|Out-Null
if($SourceRef){
    $header=(& git -C $repo show "${SourceRef}:src/ui_vertex_shadow.h") -join "`n"
    if($LASTEXITCODE -ne 0){throw 'Source header unavailable'}
}else{$header=Get-Content (Join-Path $repo 'src/ui_vertex_shadow.h') -Raw}
$header|Set-Content (Join-Path $out 'selected_shadow.h') -Encoding UTF8
Copy-Item (Join-Path $PSScriptRoot 'mapped_vertex_copy_test.cpp') (Join-Path $out 'test.cpp') -Force
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$repo\src`" `"$out\test.cpp`" /Fo:`"$out\test.obj`" /Fe:`"$out\test.exe`""
if($LASTEXITCODE -ne 0){throw 'Mapped vertex copy fixture build failed'}
& (Join-Path $out 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Mapped vertex copy fixture failed'}
