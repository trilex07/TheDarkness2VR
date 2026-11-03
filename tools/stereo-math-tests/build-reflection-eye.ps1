$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out=Join-Path $root 'test-output/reflection-eye-tests'
New-Item -ItemType Directory -Force $out | Out-Null
$source=Get-Content (Join-Path $root 'src/stereo_replay.inl') -Raw
$start=$source.IndexOf('struct ReflectionEyeScope {')
$end=$source.IndexOf('// Keep C++ restoration scopes outside', $start)
if($start -lt 0 -or $end -le $start){throw 'Missing production scope'}
[IO.File]::WriteAllText((Join-Path $out 'reflection_scope_actual.inl'),$source.Substring($start,$end-$start))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$PSScriptRoot/reflection_eye_test.cpp`" /Fo:reflection-eye.obj /Fe:reflection-eye.exe"
 if($LASTEXITCODE){throw 'Compile failed'}
 & .\reflection-eye.exe
 if($LASTEXITCODE){throw 'Test failed'}
}finally{Pop-Location}
