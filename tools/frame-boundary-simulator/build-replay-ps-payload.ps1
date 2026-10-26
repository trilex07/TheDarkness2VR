$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$src=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$start=$src.IndexOf('        case OP_SetPSConstF: {',[StringComparison]::Ordinal)
$end=$src.IndexOf('        case OP_SetPSConstI:', $start,[StringComparison]::Ordinal)
if($start -lt 0 -or $end -le $start){throw 'Actual replay PS source boundaries missing'}
$out=Join-Path $env:TEMP 'd2vr-replay-ps-payload'
New-Item -ItemType Directory -Path $out -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $out 'actual_replay_ps_case.inl'),$src.Substring($start,$end-$start))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$here\replay_ps_payload_test.cpp`" /Fe:replay-ps-payload-test.exe"
    if($LASTEXITCODE -ne 0){throw 'build failed'}
    & .\replay-ps-payload-test.exe
    if($LASTEXITCODE -ne 0){throw 'actual PS packet regression failed'}
} finally {Pop-Location}
