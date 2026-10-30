$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-native-submission-boundaries'
New-Item -ItemType Directory -Force -Path $out|Out-Null
$source=Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw
function Extract([string]$name){
 $start=$source.IndexOf("static HRESULT STDMETHODCALLTYPE $name(")
 if($start -lt 0){throw "Missing $name"}
 $brace=$source.IndexOf('{',$start);$depth=0
 for($i=$brace;$i -lt $source.Length;++$i){
  if($source[$i] -eq '{'){++$depth}
  if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
 }
 throw "Unclosed $name"
}
[IO.File]::WriteAllText((Join-Path $out 'actual_native_boundaries.inl'),(Extract 'SubmissionDeviceQueryInterface')+"`n"+(Extract 'SubmissionSetSoftwareProcessing'))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$out`" `"$PSScriptRoot\native_submission_boundaries_test.cpp`" /Fo:`"$out\test.obj`" /Fe:`"$out\test.exe`""
if($LASTEXITCODE){throw 'Native boundaries x86 build failed'}
foreach($mode in 'unknown','alias','software'){
 & "$out\test.exe" $mode
 if($LASTEXITCODE){throw "Native boundary failed: $mode"}
}
