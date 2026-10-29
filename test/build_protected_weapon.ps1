$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-protected-weapon'
New-Item -ItemType Directory -Force -Path $out | Out-Null
function Extract([string]$text,[string]$signature,[bool]$structure=$false) {
    $start=$text.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$text.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $text.Length;++$i){
        if($text[$i] -eq '{'){++$depth}
        if($text[$i] -eq '}'){--$depth;if(!$depth){return $text.Substring($start,$i-$start+1)+$(if($structure){';'}else{''})}}
    }
    throw "Unclosed $signature"
}
$aim=Get-Content (Join-Path $repo 'src/native_aim.inl') -Raw
$body=(Extract $aim 'struct NativeGameplaySample {' $true)+"`r`nstatic NativeGameplaySample g_nativeGameplay;`r`n"+
    (Extract $aim 'static bool CopyNativeGameplay(')+"`r`n"+(Extract $aim 'static bool NativeAimOwnerMatches(')+"`r`n"+
    (Extract $aim 'static bool CaptureNativeGameplay(')+"`r`n"+(Extract $aim 'static bool NativeTentacleHeadAttachmentAllowed(')
[IO.File]::WriteAllText((Join-Path $out 'actual_weapon_owner.inl'),$body,[Text.Encoding]::UTF8)
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$fixture=Join-Path $PSScriptRoot 'native_weapon_bridge_fixture.cpp'
$source=Join-Path $PSScriptRoot 'protected_weapon_test.cpp'
$nativeBody=Join-Path $out 'native-fire.bin'
python -B (Join-Path $PSScriptRoot 'native_weapon_prefix_plan.py') --exe 'C:\Program Files (x86)\Steam\steamapps\common\Darkness II\DarknessII.exe' --check-header (Join-Path $repo 'src/native_weapon_prefix_plan.h') --fire-fixture-body $nativeBody
if($LASTEXITCODE -ne 0){throw 'Native fire contract refused'}
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /LD /EHsc /W4 /O2 /MD /std:c++17 `"$fixture`" /Fe:fixture.dll /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'Weapon ABI fixture build failed'}
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk/include`" /I`"$out`" `"$source`" /Fe:protected_weapon.exe /link bcrypt.lib"
    if($LASTEXITCODE -ne 0){throw 'Protected weapon test build failed'}
    & .\protected_weapon.exe (Join-Path $out 'fixture.dll') $nativeBody
    if($LASTEXITCODE -ne 0){throw 'Protected weapon regression failed'}
} finally {Pop-Location}
