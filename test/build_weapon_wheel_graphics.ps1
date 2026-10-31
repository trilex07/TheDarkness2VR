# Offline actual-header geometry regression. Never loads the game or proxy DLL.
param([switch]$Preview)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/weapon-wheel-graphics'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC x86 toolchain not found'}
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'weapon_wheel_graphics_test.cpp'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 `"$source`" /Fe:weapon-wheel-graphics-test.exe"
 if($LASTEXITCODE -ne 0){throw 'Weapon wheel geometry compile failed'}
 if($Preview){& './weapon-wheel-graphics-test.exe' (Join-Path $out 'wheel-preview.svg')}
 else {& './weapon-wheel-graphics-test.exe'}
 if($LASTEXITCODE -ne 0){throw 'Weapon wheel geometry regression failed'}
} finally {Pop-Location}
