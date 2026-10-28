$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-protected-hands'
New-Item -ItemType Directory -Force -Path $out | Out-Null
function Extract([string]$text,[string]$signature,[bool]$structure=$false) {
    $start=$text.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$text.IndexOf('{',$start);$depth=0;$end=-1
    for($i=$brace;$i -lt $text.Length;++$i){
        if($text[$i] -eq '{'){++$depth}
        if($text[$i] -eq '}'){--$depth;if(!$depth){$end=$i;break}}
    }
    if($end -lt 0){throw "Unclosed $signature"}
    return $text.Substring($start,$end-$start+1)+$(if($structure){';'}else{''})
}
$aim=Get-Content (Join-Path $repo 'src/native_aim.inl') -Raw
$weapon=Get-Content (Join-Path $repo 'src/native_weapon.inl') -Raw
$types=(Extract $aim 'struct NativeGameplaySample {' $true)+"`r`n"+(Extract $weapon 'struct NativeWeaponPose {' $true)
$helpers=(Extract $aim 'static bool CopyNativeGameplay(')+"`r`n"+(Extract $aim 'static bool NativeAimOwnerMatches(')
[IO.File]::WriteAllText((Join-Path $out 'actual_hands_types.inl'),$types,[Text.Encoding]::UTF8)
[IO.File]::WriteAllText((Join-Path $out 'actual_hands_owner.inl'),$helpers,[Text.Encoding]::UTF8)
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$source=Join-Path $PSScriptRoot 'protected_hands_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk/include`" /I`"$out`" `"$source`" /Fe:protected_hands.exe"
    if($LASTEXITCODE -ne 0){throw 'Protected hand fixture build failed'}
    & .\protected_hands.exe
    if($LASTEXITCODE -ne 0){throw 'Protected hand regression failed'}
} finally {Pop-Location}
