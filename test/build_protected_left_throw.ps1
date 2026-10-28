$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
function Extract([string]$source,[string]$signature) {
    $start=$source.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$source.IndexOf('{',$start)
    if(-not $signature.StartsWith('struct ')) {
        $header=[regex]::Match($source.Substring($start),'\)\s*\{')
        if(-not $header.Success){throw "Missing function body $signature"}
        $brace=$start+$header.Index+$header.Length-1
    }
    $depth=0
    for($i=$brace;$i -lt $source.Length;++$i){
        if($source[$i] -eq '{'){++$depth}
        if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
    }
    throw "Unclosed $signature"
}
$main=Get-Content (Join-Path $repo 'src\d3d9.cpp') -Raw
$aim=Get-Content (Join-Path $repo 'src\native_aim.inl') -Raw
$throw=Get-Content (Join-Path $repo 'src\native_throw.inl') -Raw
$protected=Get-Content (Join-Path $repo 'src\protected_left_throw.inl') -Raw
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_protected_throw_types.inl'),(Extract $throw 'struct NativeThrowToken {')+';',[Text.Encoding]::UTF8)
$bodies=@((Extract $main 'static XrVector3f RotVec('),(Extract $aim 'static bool NativeAimOwnerMatches('),
    (Extract $throw 'static void ResetNativeThrow() {'),(Extract $throw 'static bool NativeThrowFireOwnerMatches('),
    (Extract $protected 'static bool ProtectedThrowAvailable() {'),
    (Extract $protected 'static bool ProtectedLeftAimHeld() {'),(Extract $protected 'static void ResetProtectedLeftThrow() {'),
    (Extract $throw 'static void PublishNativeThrow('),(Extract $throw 'static bool TryNativeThrowVelocity('),
    (Extract $protected 'static bool ResolveProtectedThrowVelocity('),(Extract $protected 'static void ApplyProtectedLeftThrow('))
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_protected_throw_under_test.inl'),($bodies -join "`r`n"),[Text.Encoding]::UTF8)
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$source=Join-Path $PSScriptRoot 'protected_left_throw_test.cpp'
$out=Join-Path $env:TEMP 'd2vr-protected-left-throw.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk\include`" /I`"$env:TEMP`" `"$source`" /Fo:`"$env:TEMP\d2vr-protected-left-throw.obj`" /Fe:`"$out`""
if($LASTEXITCODE -ne 0){throw 'Protected left throw build failed'}
& $out
if($LASTEXITCODE -ne 0){throw 'Protected left throw regression failed'}
