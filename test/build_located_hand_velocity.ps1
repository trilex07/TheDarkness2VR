param([string]$SourceRef)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$source=if($SourceRef){(& git -C $repo show "${SourceRef}:src/d3d9.cpp") -join "`n"}else{Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw}
$start=$source.IndexOf('static void LocateTrackedHands() {')
if($start -lt 0){throw 'LocateTrackedHands definition missing'}
$brace=$source.IndexOf('{',$start);$depth=0;$end=-1
for($i=$brace;$i -lt $source.Length;++$i){
 if($source[$i] -eq '{'){++$depth}
 if($source[$i] -eq '}'){--$depth;if(!$depth){$end=$i;break}}
}
if($end -lt 0){throw 'Unclosed LocateTrackedHands definition'}
$out=Join-Path $env:TEMP 'd2vr-located-hand-velocity'
New-Item -ItemType Directory -Force -Path $out | Out-Null
[IO.File]::WriteAllText((Join-Path $out 'actual_located_hands.inl'),$source.Substring($start,$end-$start+1))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$test=Join-Path $PSScriptRoot 'located_hand_velocity_test.cpp'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk/include`" /I`"$out`" `"$test`" /Fe:test.exe"
 if($LASTEXITCODE -ne 0){throw 'Located hand test build failed'}
 & .\test.exe
 if($LASTEXITCODE -ne 0){throw 'Located hand regression failed'}
} finally {Pop-Location}
