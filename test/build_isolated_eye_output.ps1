$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$out=Join-Path $env:TEMP 'd2vr-isolated-eye-output'
New-Item -ItemType Directory -Force -Path $out|Out-Null
$source=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
$start=$source.IndexOf('enum RecOp : uint16_t {');$end=$source.IndexOf('// State blocks can bind',$start)
if($start -lt 0 -or $end -lt 0){throw 'Actual recorder declarations missing'}
[IO.File]::WriteAllText((Join-Path $out 'actual_records.inl'),$source.Substring($start,$end-$start))
$snapshot=[regex]::Match($source,'(?s)struct DeviceSnapshot \{.*?\r?\n\};')
if(!$snapshot.Success){throw 'Actual snapshot missing'}
[IO.File]::WriteAllText((Join-Path $out 'actual_snapshot.inl'),$snapshot.Value)
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$test=Join-Path $PSScriptRoot 'isolated_eye_output_test.cpp'
Push-Location $out
try{
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$out`" `"$test`" /Fe:test.exe /link d3d9.lib user32.lib"
 if($LASTEXITCODE -ne 0){throw 'Build failed'}
 & ./test.exe
 if($LASTEXITCODE -ne 0){throw 'Tests failed'}
}finally{Pop-Location}
