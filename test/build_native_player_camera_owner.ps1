$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'native_player_camera_owner_test.cpp'
$out=Join-Path $env:TEMP 'd2vr-native-player-camera-owner.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-native-player-camera-owner.obj`" /Fe:`"$out`""
if($LASTEXITCODE -ne 0){throw 'Owner fixture build failed'}
& $out
if($LASTEXITCODE -ne 0){throw 'Owner graph regression failed'}
$text=Get-Content (Join-Path $PSScriptRoot '..\src\native_aim.inl') -Raw
$bodies=@()
foreach($signature in @('static void ObserveNativeViewOwnerChecked(','static void __cdecl ObserveNativeViewOwner(')) {
    $start=$text.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$text.IndexOf('{',$start);$depth=0;$end=-1
    for($i=$brace;$i -lt $text.Length;++$i){
        if($text[$i] -eq '{'){++$depth}
        if($text[$i] -eq '}'){--$depth;if(!$depth){$end=$i;break}}
    }
    if($end -lt 0){throw "Unclosed $signature"}
    $bodies+=$text.Substring($start,$end-$start+1)
}
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_native_view_owner_under_test.inl'),($bodies -join "`r`n"),[Text.Encoding]::UTF8)
$source=Join-Path $PSScriptRoot 'native_view_owner_test.cpp'
$out=Join-Path $env:TEMP 'd2vr-native-view-owner.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$env:TEMP`" `"$source`" /Fo:`"$env:TEMP\d2vr-native-view-owner.obj`" /Fe:`"$out`""
if($LASTEXITCODE -ne 0){throw 'View owner fixture build failed'}
& $out
if($LASTEXITCODE -ne 0){throw 'View owner regression failed'}
