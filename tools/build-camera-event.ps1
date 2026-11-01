$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'build/camera-event'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$sdk='C:/Users/Trilex/openxr-sdk'
$buildCmd='"'+$vc+'" x86 && cl /nologo /LD /EHsc /W3 /O2 /Zi /MD /std:c++17 /DD2VR_MATCHED_CAPTURE=1 '+
    '/I"'+$sdk+'/include" /I"'+$root+'/src" /Fo:"'+$out+'/" /Fd:"'+$out+'/compile.pdb" /Fe:"'+$out+'/d3d9.dll" '+
    'src\d3d9.cpp src\dlss\dlss_client.cpp src\dlss\dlss_gpu.cpp third_party\minhook\src\buffer.c third_party\minhook\src\hook.c third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde32.c '+
    '/link /DEBUG /PDB:"'+$out+'/d3d9.pdb" /IMPLIB:"'+$out+'/d3d9.lib" /OPT:REF /OPT:ICF /MAP:"'+$out+'/d3d9.map" /DEF:d3d9.def '+
    '/LIBPATH:"'+$sdk+'/native/Win32/release/lib" openxr_loader.lib dxgi.lib d3d11.lib user32.lib gdi32.lib shell32.lib xinput.lib'
Push-Location $root
try {cmd /c $buildCmd;if($LASTEXITCODE){throw 'Isolated event build failed'}}finally{Pop-Location}
Get-FileHash -LiteralPath (Join-Path $out 'd3d9.dll') -Algorithm SHA256
