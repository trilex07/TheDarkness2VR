$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$source=Get-Content (Join-Path $here '..\..\src\d3d9.cpp') -Raw
function Body([string]$signature) {
    $start=$source.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing definition $signature"}
    $brace=$source.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $source.Length;++$i){
        if($source[$i] -eq '{'){++$depth}
        if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
    }
    throw "Unclosed function $signature"
}
$signatures=@('static XrVector3f RotVec(', 'static XrPosef HeadCenterPose(',
    'static float ComputeHorizontalYaw(', 'static SHORT StickShort(',
    'static void ApplyProtectedGestureActions(XINPUT_GAMEPAD& gp) {',
    'static WORD GestureButtons(const XINPUT_GAMEPAD& gp) {')
$bodies=@($signatures | ForEach-Object {Body $_})
$safe=Body 'static void ApplyProtectedGestureActions(XINPUT_GAMEPAD& gp) {'
if($safe -match 'PublishNative|ReadNative|CopyNative|ApplyTentacleGestures|VirtualProtect'){throw 'Protected gesture path may not call native hooks'}
if($source -notmatch 'if\(!g_protectedGestureActions\)\s*#endif\s*ApplyTentacleGestures\(gp\);'){throw 'Protected mode must bypass native gesture path'}
$fixture=Join-Path $env:TEMP 'd2vr_protected_gestures_under_test.inl'
[IO.File]::WriteAllText($fixture,($bodies -join "`r`n"),[Text.Encoding]::UTF8)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$out=Join-Path $env:TEMP 'd2vr-protected-gestures.exe'
$obj=Join-Path $env:TEMP 'd2vr-protected-gestures.obj'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /DD2VR_MATCHED_CAPTURE=1 /I`"$sdk\include`" /I`"$env:TEMP`" protected_gestures_test.cpp /Fo:`"$obj`" /Fe:`"$out`""
    if($LASTEXITCODE -ne 0){throw 'protected gesture build failed'}
    & $out
    if($LASTEXITCODE -ne 0){throw 'protected gesture regression failed'}
} finally {Pop-Location}
