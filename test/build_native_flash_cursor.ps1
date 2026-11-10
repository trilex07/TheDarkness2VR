param([string]$OutputDir='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$OutputDir){$OutputDir=Join-Path $root 'test-output/native-flash-cursor-v1'}
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
python -B (Join-Path $root 'tools/prepare_native_flash_fixture.py') --exe 'C:/Program Files (x86)/Steam/steamapps/common/Darkness II/DarknessII.exe' --out $OutputDir --com-receipt 'C:/Users/Trilex/Tiny Tina VR/captures/darkness-mouse-com-main-20261110/run/mouse-com-once.json'
if($LASTEXITCODE){throw 'Native fixture contract failed'}
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:/Users/Trilex/openxr-sdk'}
$source=Join-Path $PSScriptRoot 'native_flash_cursor_test.cpp'
Push-Location $OutputDir
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk/include`" /I`"$root/src`" /I`"$OutputDir`" `"$source`" /Fe:native_flash_cursor_test.exe /link user32.lib bcrypt.lib"
    if($LASTEXITCODE){throw 'CPU test compile failed'}
    & ./native_flash_cursor_test.exe ./transform.bin ./position.bin --legacy
    if($LASTEXITCODE -ne 1){throw 'Expected legacy RED exit1'}
    & ./native_flash_cursor_test.exe ./transform.bin ./position.bin
    if($LASTEXITCODE){throw 'Production GREEN failed'}
} finally {Pop-Location}
