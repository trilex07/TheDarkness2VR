param([string]$OutputDir)
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
if(!$OutputDir){$OutputDir=Join-Path $here '../test-output/shake-cpu'}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$OutputDir=(Resolve-Path -LiteralPath $OutputDir).Path
$source=Get-Content -LiteralPath (Join-Path $here '../src/d3d9.cpp') -Raw
$start=$source.IndexOf('static bool DoubleShakeReloadGesture(')
$end=$source.IndexOf('static WORD GestureButtons(',$start)
if($start -lt 0 -or $end -le $start){throw 'Missing actual production shake wrapper'}
[IO.File]::WriteAllText((Join-Path $OutputDir 'actual_shake_wrapper.inl'),$source.Substring($start,$end-$start))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$test=Join-Path $here 'double_shake_reload_test.cpp'
Push-Location $OutputDir
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$OutputDir`" `"$test`" /Fe:shake.exe"
    if($LASTEXITCODE){throw 'Double-shake CPU fixture build failed'}
    & (Join-Path $OutputDir 'shake.exe')
    if($LASTEXITCODE){throw 'Double-shake CPU fixture failed'}
} finally {Pop-Location}
