param([switch]$Hardware,[string]$GameplayManifest)
if($GameplayManifest -and -not $Hardware){throw 'Gameplay manifest replay requires -Hardware'}
$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$repo=Split-Path -Parent $here
$output=Join-Path $env:TEMP 'd2vr-pipeline-warmup-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null

$renderer=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
$hook=[regex]::Match($renderer,'(?s)void HookDevice\(IDirect3DDevice9\* dev\) \{.*?\r?\n\}')
if(-not $hook.Success){throw 'HookDevice body missing'}
$body=$hook.Value
$pool=$body.IndexOf('d2vr_shader_preload::Initialize(dev);')
$warm=$body.IndexOf('StartupPipelineWarmup(dev);')
$patch=$body.IndexOf('PatchVTable(dev')
if($pool -lt 0 -or $warm -lt $pool -or $patch -lt $warm){throw 'Warmup must follow pool creation and precede every device vtable patch'}
if($renderer -notmatch 'GetPrivateProfileIntA\("Render","PipelineWarmup",0,'){throw 'PipelineWarmup must default OFF'}
if($renderer -notmatch 'd2vr_pipeline_warmup::Tick\(\);'){throw 'Present-thread manifest tick missing'}

$replay=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
foreach($name in 'DrawPrimitiveWithDiffuse','DrawIndexedPrimitiveWithDiffuse','DrawPrimitiveUPWithDiffuse','DrawIndexedPrimitiveUPWithDiffuse'){
    $fn=[regex]::Match($replay,"(?s)static HRESULT $name\(.*?\r?\n\}")
    if(-not $fn.Success){throw "$name missing"}
    $observe=$fn.Value.IndexOf('if(g_replayEye<0) d2vr_pipeline_warmup::ObserveDraw(dev,g_boundVs,g_boundPs);')
    $draw=$fn.Value.IndexOf('g_orig.Draw')
    if($observe -lt 0 -or $draw -lt $observe){throw "$name must learn live draws before forwarding"}
}
foreach($setter in 'g_boundVs=shader;','g_boundPs=identity;'){
    if(-not $replay.Contains($setter)){throw "binding identity update missing: $setter"}
}

# Persistence must never serialize the whole archive in Present-thread Tick.
$warmSource=Get-Content -LiteralPath (Join-Path $repo 'src\pipeline_warmup_d3d9.inl') -Raw
$tickBody=[regex]::Match($warmSource,'(?s)static void Tick\(.*?\r?\n\}')
$writerBody=[regex]::Match($warmSource,'(?s)static unsigned __stdcall WriteManifest\(.*?\r?\n\}')
if(-not $tickBody.Success -or $tickBody.Value.Contains('.Serialize(')){throw 'Serialization must stay off the render thread'}
if(-not $writerBody.Success -or -not $writerBody.Value.Contains('manifest.Serialize(job->version)')){throw 'Writer snapshot missing'}

$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $here 'pipeline_warmup_test.cpp'
Push-Location $output
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fe:pipeline_warmup_test.exe /link user32.lib shell32.lib d3dcompiler.lib"
 if($LASTEXITCODE -ne 0){throw 'compile failed'}
 if($GameplayManifest){& .\pipeline_warmup_test.exe --hardware $GameplayManifest}elseif($Hardware){& .\pipeline_warmup_test.exe --hardware}else{& .\pipeline_warmup_test.exe}
 if($LASTEXITCODE -ne 0){throw 'pipeline warmup regression failed'}
 Write-Host 'PASS actual-source ordering: pool -> warmup -> vtable patch; live-only learning in all four draw wrappers; default OFF'
} finally {Pop-Location}
