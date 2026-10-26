$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Get-Content (Join-Path $here '../../src/stereo_replay.inl') -Raw
$start = $source.IndexOf('static bool ApplyInverseProjection(')
$end = $source.IndexOf('// Combined per-eye PS constant compensation:', $start)
if ($start -lt 0 -or $end -lt 0) { throw 'Missing actual refresh function' }
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_tracking_inverse.inl'), $source.Substring($start,$end-$start))
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /I`"$env:TEMP`" protected_head_tracking_test.cpp /Fo:`"$env:TEMP/d2vr-protected-head.obj`" /Fe:`"$env:TEMP/d2vr-protected-head.exe`""
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    & "$env:TEMP/d2vr-protected-head.exe"
    if ($LASTEXITCODE -ne 0) { throw 'regression failed' }
} finally { Pop-Location }
