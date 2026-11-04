$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Body($text,$signature) {
 $start=$text.IndexOf($signature); if($start -lt 0){throw "Missing $signature"}
 $brace=$text.IndexOf('{',$start);$depth=0
 for($i=$brace;$i -lt $text.Length;++$i){
  if($text[$i] -eq '{'){++$depth};if($text[$i] -eq '}'){--$depth;if(!$depth){return $text.Substring($start,$i-$start+1)}}
 };throw 'Unclosed body'
}
$aim=Get-Content "$root/src/protected_left_aim.inl" -Raw
$main=Get-Content "$root/src/d3d9.cpp" -Raw
$bodies=@((Body $main 'static XrVector3f RotVec('),(Body $aim 'static bool ResolveProtectedLeftAim('),
 (Body $aim 'static void PublishProtectedLeftAim('))
[IO.File]::WriteAllText("$env:TEMP/d2vr-tentacle-aim-under-test.inl",($bodies -join "`r`n"))
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:/Users/Trilex/openxr-sdk'}
Push-Location $root
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /I`"$sdk/include`" /I`"$env:TEMP`" tools/stereo-math-tests/tentacle_comfort_test.cpp /Fo:build/tentacle-comfort.obj /Fe:build/tentacle-comfort.exe"
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & ./build/tentacle-comfort.exe
 if($LASTEXITCODE -ne 0){throw 'Regression failed'}
}finally{Pop-Location}
