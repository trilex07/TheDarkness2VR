# Offline host test for dual_weapon_wheel.h + weapon_wheel_raster.h (MSVC x86, /W4 /WX).
# Never loads the game or the proxy DLL. Optional -Preview writes a PPM of both wheels.
param([switch]$Preview)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/dual-weapon-wheel'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC x86 toolchain not found'}
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'dual_weapon_wheel_test.cpp'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /D_CRT_SECURE_NO_WARNINGS `"$source`" /Fe:dual-weapon-wheel-test.exe"
 if($LASTEXITCODE -ne 0){throw 'Dual weapon wheel test compile failed'}
 if($Preview){& './dual-weapon-wheel-test.exe' (Join-Path $out 'wheels.ppm')} else {& './dual-weapon-wheel-test.exe'}
 if($LASTEXITCODE -ne 0){throw 'Dual weapon wheel test failed'}
} finally {Pop-Location}
