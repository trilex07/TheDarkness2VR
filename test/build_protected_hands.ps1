param([switch]$NativePacker,[string]$GameExe='C:\Program Files (x86)\Steam\steamapps\common\Darkness II\DarknessII.exe')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-protected-hands'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$fixtureArgs=@()
if($NativePacker) {
    $packFile=Join-Path $out 'native-skin-pack.bin'
    # Never commit native bytes. Extract only this audited, position-independent
    # arithmetic leaf from the exact locally owned executable into private TEMP.
    @'
import sys,hashlib
from pathlib import Path
repo,exe,destination=map(Path,sys.argv[1:])
sys.path.insert(0,str(repo/'tools/native-camera'))
from audit_camera import PE
data=exe.read_bytes()
assert hashlib.sha256(data).hexdigest()=='d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1','EXE mismatch'
code=PE(data).at_va(0xb1e500,0x141)
assert hashlib.sha256(code).hexdigest()=='f2a0e5be962d866e99684fe13410075c3a785fb8654169188cb130d4e23681a6','native arithmetic leaf mismatch'
destination.write_bytes(code)
'@ | python -B - $repo $GameExe $packFile
    if($LASTEXITCODE -ne 0){throw 'Exact native packer extraction refused'}
    $fixtureArgs=@($packFile)
}
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
[IO.File]::WriteAllText((Join-Path $out 'actual_cinematic_gate.inl'),(Extract $aim 'static bool NativeCinematicPlaying('),[Text.Encoding]::UTF8)
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
    & .\protected_hands.exe @fixtureArgs
    if($LASTEXITCODE -ne 0){throw 'Protected hand regression failed'}
} finally {Pop-Location}
