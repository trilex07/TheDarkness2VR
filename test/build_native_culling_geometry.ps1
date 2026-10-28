$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
foreach($name in 'native_culling_cover_test','native_stereo_culling_test') {
    $source=Join-Path $PSScriptRoot ($name+'.cpp')
    $exe=Join-Path $env:TEMP ($name+'.exe')
    $obj=Join-Path $env:TEMP ($name+'.obj')
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fo:`"$obj`" /Fe:`"$exe`""
    if($LASTEXITCODE -ne 0){throw 'Culling geometry build failed'}
    $global:LASTEXITCODE=$null
    & $exe
    if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'Culling geometry failed or did not start'}
}
