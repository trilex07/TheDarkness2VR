param([string]$OutputDir='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$OutputDir){$OutputDir=Join-Path $root 'test-output/horizon-input-v1'}
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
python -B (Join-Path $root 'tools/prepare_horizon_input_fixture.py') --exe 'C:/Program Files (x86)/Steam/steamapps/common/Darkness II/DarknessII.exe' --out $OutputDir
if($LASTEXITCODE){throw 'Native fixture contract failed'}
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'native_horizon_input_test.cpp'
Push-Location $OutputDir
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$OutputDir`" /I`"$root/src`" `"$source`" /Fe:native_horizon_input_test.exe"
    if($LASTEXITCODE){throw 'CPU test compile failed'}
    & ./native_horizon_input_test.exe
    if($LASTEXITCODE){throw 'Native input sequence failed'}
} finally {Pop-Location}
