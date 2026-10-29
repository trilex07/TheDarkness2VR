param([switch]$Hardware,[switch]$CpuOnly,[string]$SourceRef)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP ('d2vr-state-snapshot-tests-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out -Force|Out-Null
$text=if($SourceRef){(git -C $repo show "${SourceRef}:src/stereo_replay.inl") -join "`n"}else{Get-Content -LiteralPath "$repo\src\stereo_replay.inl" -Raw}
$body=[regex]::Match($text,'(?s)struct DeviceSnapshot \{.*?\r?\n\};')
if(-not $body.Success){throw 'Actual snapshot source missing'}
$body.Value|Set-Content -LiteralPath "$out\actual_snapshot.inl" -Encoding utf8
$body.Value.Replace('IDirect3DDevice9','FakeDevice').Replace('IDirect3DStateBlock9','FakeState').Replace('IDirect3DSurface9','FakeSurface')|Set-Content -LiteralPath "$out\actual_snapshot_cpu.inl" -Encoding utf8
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'state_snapshot_test.cpp'
if($CpuOnly){$source=Join-Path $PSScriptRoot 'state_snapshot_receipt_test.cpp'}
$exe=if($CpuOnly){'state_snapshot_receipt_test.exe'}else{'state_snapshot_test.exe'}
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /std:c++17 /I. `"$source`" /Fe:$exe /link user32.lib"
 if($LASTEXITCODE -ne 0){throw 'Snapshot test compile failed'}
 if($CpuOnly){cmd /c $exe}
 elseif($Hardware){& .\state_snapshot_test.exe --hardware}else{& .\state_snapshot_test.exe}
 if($LASTEXITCODE -ne 0){throw 'Snapshot test failed'}
} finally {Pop-Location}
