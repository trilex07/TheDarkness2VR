param([string]$SourceRef='18d1a69cb3950e7840fc957707aa347f0e3cd03a')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$resolved=(& git -C $repo rev-parse --verify ($SourceRef+'^{commit}')).Trim()
if($LASTEXITCODE -ne 0 -or $resolved -notmatch '^[0-9a-f]{40}$'){throw 'Invalid source commit'}
$probeRoot=Join-Path $env:TEMP ('d2vr-variant-review-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $probeRoot | Out-Null
$archive=Join-Path $probeRoot 'source.tar'
& git -C $repo archive --format=tar --output=$archive $resolved src test/pipeline_warmup_test.cpp
if($LASTEXITCODE -ne 0){throw 'archive failed'}
& tar -xf $archive -C $probeRoot
if($LASTEXITCODE -ne 0){throw 'extract failed'}
$source=Join-Path $probeRoot 'test/pipeline_warmup_test.cpp'
$text=Get-Content -LiteralPath $source -Raw
$marker='        // Per-frame capture budget: excess keys are deferred, never marked seen.'
if(-not $text.Contains($marker)){throw 'Reference is not the reviewed variant learner'}
$repro=@"
        HR(device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_LESS));
        IDirect3DStateBlock9* opaque=nullptr; HR(device->CreateStateBlock(D3DSBT_ALL,&opaque));
        HR(device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER));
        TrackRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);
        HR(device->BeginScene()); ObserveDraw(device,gameVs,gamePs); HR(device->EndScene());
        const auto beforeOpaque=variants.manifest.Size();
        HR(opaque->Apply()); opaque->Release();
        HR(device->BeginScene()); ObserveDraw(device,gameVs,gamePs); HR(device->EndScene());
        CHECK(variants.manifest.Size()==beforeOpaque+1);
"@
Set-Content -LiteralPath $source -Value $text.Replace($marker,$repro+[Environment]::NewLine+$marker)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $probeRoot
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 test\pipeline_warmup_test.cpp /Fe:probe.exe /link user32.lib shell32.lib d3dcompiler.lib"
    if($LASTEXITCODE -ne 0){throw 'compile failed'}
    $output=& .\probe.exe 2>&1
    $exitCode=$LASTEXITCODE
    $output | Set-Content -LiteralPath (Join-Path $probeRoot 'repro.log')
    if($exitCode -ne 1 -or ($output -join "`n") -notmatch 'FAIL \d+: variants.manifest.Size\(\)==beforeOpaque\+1'){
        throw "Expected missing-variant assertion was not reproduced: exit=$exitCode"
    }
    $global:LASTEXITCODE=0 # expected native assertion failure was handled above
    Write-Host "CONFIRMED source defect at ${resolved}: StateBlock ALPHAFUNC variant is missed."
    Write-Host "This is an expected FAIL reproduction, not a passing integration regression. Evidence: $probeRoot"
} finally {Pop-Location}
