$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out=Join-Path $root 'build/cinematic-hands'
New-Item -ItemType Directory -Force $out | Out-Null
$hands=Get-Content (Join-Path $root 'src/native_hands.inl') -Raw
$start=$hands.IndexOf('struct NativeHandsPaletteCache {')
$end=$hands.IndexOf('static bool ApplyNativeHandsChecked(', $start)
[IO.File]::WriteAllText((Join-Path $out 'cinematic_restore_actual.inl'),$hands.Substring($start,$end-$start))
$gateStart=$hands.IndexOf('    if(!protectedRoute && (sample.grab==')
$gateEnd=$hands.IndexOf('    float gunWorld[16]', $gateStart)
if($gateStart -lt 0 -or $gateEnd -lt 0){throw 'Missing production grab admission'}
[IO.File]::WriteAllText((Join-Path $out 'hands_grab_actual.inl'),$hands.Substring($gateStart,$gateEnd-$gateStart))
$pack=Get-Content (Join-Path $root 'src/protected_hands.inl') -Raw
$start=$pack.IndexOf('using ProtectedHandsPackMethod=')
$end=$pack.IndexOf('static void PrepareProtectedHandsForCamera(', $start)
[IO.File]::WriteAllText((Join-Path $out 'cinematic_pack_actual.inl'),$pack.Substring($start,$end-$start))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $root
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /I`"$out`" tools\stereo-math-tests\cinematic_hands_test.cpp /Fo:`"$out\test.obj`" /Fe:`"$out\test.exe`""
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & (Join-Path $out 'test.exe')
 if($LASTEXITCODE -ne 0){throw 'Regression failed'}
} finally {Pop-Location}
