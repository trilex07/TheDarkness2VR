$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
function Extract([string]$source,[string]$signature) {
    $start=$source.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$source.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $source.Length;++$i){
        if($source[$i] -eq '{'){++$depth}
        if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
    }
    throw "Unclosed $signature"
}
$main=Get-Content (Join-Path $repo 'src\d3d9.cpp') -Raw
$aim=Get-Content (Join-Path $repo 'src\native_aim.inl') -Raw
$protected=Get-Content (Join-Path $repo 'src\protected_left_aim.inl') -Raw
$body=(Extract $main 'static XrVector3f RotVec(') + "`r`n" + (Extract $aim 'struct NativeAimSnapshot {') + ";`r`n" +
    (Extract $protected 'struct ProtectedLeftRay {') + ";`r`nstatic ProtectedLeftRay g_protectedAimRay;`r`n" +
    (Extract $protected 'static void ResetProtectedLeftAim() {') + "`r`n" +
    (Extract $protected 'static bool ResolveProtectedLeftAim(') + "`r`n" +
    (Extract $protected 'static void PublishProtectedLeftAim(')
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_protected_left_aim_under_test.inl'),$body,[Text.Encoding]::UTF8)
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$source=Join-Path $PSScriptRoot 'protected_left_aim_test.cpp'
$out=Join-Path $env:TEMP 'd2vr-protected-left-aim.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk\include`" /I`"$env:TEMP`" `"$source`" /Fo:`"$env:TEMP\d2vr-protected-left-aim.obj`" /Fe:`"$out`""
if($LASTEXITCODE -ne 0){throw 'Protected left aim build failed'}
& $out
if($LASTEXITCODE -ne 0){throw 'Protected left aim regression failed'}
