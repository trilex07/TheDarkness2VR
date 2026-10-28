$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$source=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
$start=$source.IndexOf('struct TentacleHeadScope {')
$end=$source.IndexOf('// Keep C++ restoration scopes', $start)
if($start -lt 0 -or $end -lt 0){throw 'Actual production scope not found'}
$out=Join-Path $env:TEMP 'd2vr-tentacle-head-scope'
New-Item -ItemType Directory -Force -Path $out|Out-Null
[IO.File]::WriteAllText((Join-Path $out 'actual_tentacle_head_scope.inl'),$source.Substring($start,$end-$start))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$test=Join-Path $PSScriptRoot 'tentacle_head_scope_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$test`" /Fe:test.exe"
    if($LASTEXITCODE -ne 0){throw 'Head scope build failed'}
    & .\test.exe
    if($LASTEXITCODE -ne 0){throw 'Head scope regression failed'}
} finally {Pop-Location}
