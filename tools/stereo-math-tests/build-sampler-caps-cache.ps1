$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = [IO.Path]::GetFullPath((Join-Path $here '..\..'))
# Lifecycle integration assertions complement the actual compiled cache tests.
$renderer = Get-Content -LiteralPath (Join-Path $root 'src\d3d9.cpp') -Raw
foreach ($signature in @(
    'HRESULT STDMETHODCALLTYPE Hook_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {',
    'HRESULT STDMETHODCALLTYPE Hook_ResetEx(IDirect3DDevice9Ex* dev, D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* dm) {',
    'void HookDevice(IDirect3DDevice9* dev) {'
)) {
    $pattern = [regex]::Escape($signature) + '\s*d2vr_sampling::InvalidateSamplerCaps\(\);'
    if (-not [regex]::IsMatch($renderer, $pattern)) { throw "Missing unconditional entry invalidation: $signature" }
}
$replay = Get-Content -LiteralPath (Join-Path $root 'src\stereo_replay.inl') -Raw
$resolvePattern = '(?s)static DWORD ResolveSamplerValue\(IDirect3DDevice9\* dev, D3DSAMPLERSTATETYPE t, DWORD v\) \{(.*?)\r?\n\}'
$match = [regex]::Match($replay, $resolvePattern)
$expected = 'if(g_submitMode!=4||t!=D3DSAMP_MAXANISOTROPY)returnv;if(g_samplerCapsCacheEnabled)returnd2vr_sampling::g_samplerCapsCache.Resolve(dev,v,(DWORD)g_textureAnisotropy);D3DCAPS9caps{};if(FAILED(dev->GetDeviceCaps(&caps))||caps.MaxAnisotropy<1)returnv;returnstd::min(caps.MaxAnisotropy,std::max(v,(DWORD)g_textureAnisotropy));'
# Ignore the explanatory comment, but require the exact pre-cache OFF resolver.
if (-not $match.Success -or (($match.Groups[1].Value -replace '(?m)//[^\r\n]*', '') -replace '\s', '') -ne $expected) {
    throw 'Resolver admission or cache arguments changed'
}
$actualResolver = $match.Value
$forwardPattern = '(?s)static HRESULT STDMETHODCALLTYPE Hook_SetSamplerState\(IDirect3DDevice9\* dev, DWORD smp, D3DSAMPLERSTATETYPE t, DWORD v\) \{(.*?)\r?\n\}'
$match = [regex]::Match($replay, $forwardPattern)
$expected = 'v=ResolveSamplerValue(dev,t,v);if(g_recActive)RecEmit(OP_SetSamplerState,0,smp,t,v);returng_orig.SetSamplerState(dev,smp,t,v);'
if (-not $match.Success -or ($match.Groups[1].Value -replace '\s', '') -ne $expected) {
    throw 'Exact resolved recording/argument/HRESULT forwarding changed'
}
$actualHook = $match.Value
if (-not $renderer.Contains('bool g_samplerCapsCacheEnabled = false;') -or
    -not $renderer.Contains('g_samplerCapsCacheEnabled = GetPrivateProfileIntA("Render", "SamplerCapsCache", 0, chosenIni) != 0;')) {
    throw 'SamplerCapsCache must be explicitly OFF by default'
}
$outDir = Join-Path $env:TEMP 'd2vr-sampler-caps-cache-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
# Compile the actual source bodies with the existing test's owned COM fixture.
($actualResolver + "`n" + $actualHook) | Set-Content -LiteralPath (Join-Path $outDir 'actual_sampler_hook.inl') -Encoding utf8
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools unavailable' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$testExe = Join-Path $env:TEMP 'd2vr-sampler-caps-cache-test.exe'
$testObj = Join-Path $env:TEMP 'd2vr-sampler-caps-cache-test.obj'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$outDir`" sampler_caps_cache_test.cpp /Fo:`"$testObj`" /Fe:`"$testExe`""
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
    Write-Host 'PASS: OFF-default A/B gate, actual resolver/hook recording/HRESULT forwarding and all three unconditional lifecycle invalidations'
} finally { Pop-Location }
