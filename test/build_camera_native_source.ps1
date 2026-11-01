$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/camera-event'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $root
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W3 /O2 /MT /std:c++17 /I src test/camera_native_source_test.cpp /Fo:build/camera-event/source_test.obj /Fe:build/camera-event/source_test.exe /link bcrypt.lib && build\camera-event\source_test.exe"
    if($LASTEXITCODE){throw 'Native source fixture failed'}
    python -B tools/audit-camera-source-plan.py
    if($LASTEXITCODE){throw 'Native source binary audit failed'}
} finally {Pop-Location}
