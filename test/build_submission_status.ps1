$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$src=Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw
$begin=$src.IndexOf('    const char* psSrc =',$src.IndexOf('static bool EnsureD11StereoPipeline()'))
$end=$src.IndexOf('    ID3DBlob* vsBlob =',$begin)
if($begin -lt 0 -or $end -lt 0){throw 'Actual pixel shader missing'}
$out=Join-Path $env:TEMP ('d2vr-submission-label-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out|Out-Null
[IO.File]::WriteAllText((Join-Path $out 'actual_submission_ps.inl'),$src.Substring($begin,$end-$begin))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$test=Join-Path $PSScriptRoot 'submission_status_test.cpp'
Push-Location $out
try{
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$out`" `"$test`" /Fe:submission_status.exe /link d3d11.lib d3dcompiler.lib"
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & .\submission_status.exe
 if($LASTEXITCODE -ne 0){throw 'Pixel regression failed'}
}finally{Pop-Location}
