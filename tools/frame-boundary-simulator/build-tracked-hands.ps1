$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$source=Get-Content (Join-Path $here '..\..\src\d3d9.cpp') -Raw
function Extract([string]$signature) {
    $start=$source.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$source.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $source.Length;++$i){
        if($source[$i] -eq '{'){++$depth}
        if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
    }
    throw "Unclosed $signature"
}
$body=(Extract 'struct TrackedHand {') + ";`r`nstatic TrackedHand g_hands[2];`r`n" + (Extract 'static void LocateTrackedHands() {')
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_tracked_hands_under_test.inl'),$body,[Text.Encoding]::UTF8)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$out=Join-Path $env:TEMP 'd2vr-tracked-hands.exe'
$obj=Join-Path $env:TEMP 'd2vr-tracked-hands.obj'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /I`"$sdk\include`" /I`"$env:TEMP`" tracked_hands_test.cpp /Fo:`"$obj`" /Fe:`"$out`""
    if($LASTEXITCODE -ne 0){throw 'tracked hands build failed'}
    & $out
    if($LASTEXITCODE -ne 0){throw 'tracked hands regression failed'}
} finally {Pop-Location}
