$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\native_camera_cover.inl') -Raw
$begin=$source.IndexOf('static bool RebuildOwnedCullingFrustum(')
$end=$source.IndexOf('static bool PrepareOwnedCullingInput(', $begin)
if($begin -lt 0 -or $end -lt 0){throw 'Actual source function not found'}
$body=$source.Substring($begin,$end-$begin).Replace('0x607CD0','reinterpret_cast<uintptr_t>(&BuildPlanes)').Replace('0x685050','reinterpret_cast<uintptr_t>(&BuildBounds)')
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'native_culling_rebuild_fixture.cpp') -Raw
$header=(Join-Path $repo 'src\native_culling_cover.h').Replace('\','/')
$generated=Join-Path $env:TEMP 'd2vr-culling-rebuild.cpp'
$fixture.Replace('@HEADER@',$header).Replace('@IMPLEMENTATION@',$body) | Set-Content -LiteralPath $generated -Encoding UTF8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-culling-rebuild.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$env:TEMP\d2vr-culling-rebuild.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Culling fixture build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Culling fixture failed'}
