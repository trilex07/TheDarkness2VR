$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source=Get-Content "$root/src/stereo_replay.inl" -Raw
$start=$source.IndexOf('static const GUID kTentacleGlowDigest');$end=$source.IndexOf('// Correct NDC only',$start)
if($start -lt 0 -or $end -lt 0){throw 'Missing actual glow scope'}
[IO.File]::WriteAllText("$env:TEMP/d2vr-glow-under-test.inl",$source.Substring($start,$end-$start))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $root
try {
 New-Item -ItemType Directory -Force build | Out-Null
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /I`"$env:TEMP`" tools\stereo-math-tests\tentacle_glow_test.cpp /Fo:build\tentacle-glow-test.obj /Fe:build\tentacle-glow-test.exe"
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & .\build\tentacle-glow-test.exe
 if($LASTEXITCODE -ne 0){throw 'Regression failed'}
} finally {Pop-Location}
