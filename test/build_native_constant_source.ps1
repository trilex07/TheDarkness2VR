param([switch]$PublicPolicy)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$output=Join-Path $env:TEMP ('d2vr-native-constant-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output|Out-Null
$replay=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
$hook=[regex]::Match($replay,'(?s)#pragma optimize\("y", off\)\r?\n__declspec\(noinline\) static HRESULT STDMETHODCALLTYPE Native_SetPSConstF\(.*?D2VR_CONSTANT_GATE\(Hook_SetPSConstF,Native_SetPSConstF\)')
if(-not $hook.Success){throw 'actual frame-preserving PS hook boundary missing'}
$resultHook=[regex]::Match($replay,'(?s)static inline HRESULT RecordLiveResult\(.*?\r?\n\}')
if(-not $resultHook.Success){throw 'actual live result hook missing'}
($resultHook.Value+"`n"+$hook.Value.Replace('d2vr_native_constants::InspectCurrent(', 'InspectFixture('))|Set-Content (Join-Path $output 'actual_native_constant_hook.inl') -Encoding utf8
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC/Auxiliary/Build/vcvarsall.bat'
$policyDefine=if($PublicPolicy){"/DD2VR_NO_PRIVATE_CONSTANT_POLICY=1"}else{""}
$source=Join-Path $PSScriptRoot 'native_constant_source_test.cpp'
Push-Location $output
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 $policyDefine /I`"$output`" `"$source`" /Fe:native_constant_source_test.exe"
    if($LASTEXITCODE -ne 0){throw 'native source ABI build failed'}
    & .\native_constant_source_test.exe
    if($LASTEXITCODE -ne 0){throw 'native source ABI test failed'}
} finally {Pop-Location}
