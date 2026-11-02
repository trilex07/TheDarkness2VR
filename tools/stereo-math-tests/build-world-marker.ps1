$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$out=Join-Path $root 'build/world-marker'
New-Item -ItemType Directory -Force $out | Out-Null
$source=Get-Content (Join-Path $root 'src/world_marker_projection.inl') -Raw
$begin=$source.IndexOf('static float* __fastcall WorldMarkerProjectHook(')
$end=$source.IndexOf('static bool WorldMarkerContracts()', $begin)
if($begin -lt 0 -or $end -lt 0){throw 'Production hook not found'}
[IO.File]::WriteAllText((Join-Path $out 'world_marker_hook_actual.inl'),$source.Substring($begin,$end-$begin))
Push-Location $root
try {
 New-Item -ItemType Directory -Force build | Out-Null
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 tools\stereo-math-tests\world_marker_test.cpp /Fo:build\world-marker-test.obj /Fe:build\world-marker-test.exe"
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & .\build\world-marker-test.exe
 if($LASTEXITCODE -ne 0){throw 'Regression failed'}
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$out`" tools\stereo-math-tests\world_marker_hook_test.cpp /Fo:`"$out\hook.obj`" /Fe:`"$out\hook.exe`""
 if($LASTEXITCODE -ne 0){throw 'Hook compile failed'}
 & (Join-Path $out 'hook.exe')
 if($LASTEXITCODE -ne 0){throw 'Hook regression failed'}
} finally {Pop-Location}
