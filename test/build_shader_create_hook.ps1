param([string[]]$PrivateShaders = @())
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Split-Path -Parent $here
$source = Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw

function Extract-Source([string]$text, [string]$begin, [string]$end) {
    $start = $text.IndexOf($begin)
    $finish = $text.IndexOf($end, $start + $begin.Length)
    if ($start -lt 0 -or $finish -lt 0) { throw "Shader hook source boundary missing: $begin" }
    return $text.Substring($start, $finish - $start)
}

$helpers = (Extract-Source $source 'static int FindNamedRegister(' 'static inline uint32_t RegKey') +
    (Extract-Source $source 'static uint32_t HashBytes(' '// Detects row-vector') +
    (Extract-Source $source 'static size_t SafeShaderTokenCount(' '// ------------------------------------------------------------- stereo constant injection') +
    (Extract-Source $source 'static bool BuildUnfilteredFxaa(' 'static HRESULT STDMETHODCALLTYPE Hook_CreatePixelShader(')
$hook = Extract-Source $source 'static HRESULT STDMETHODCALLTYPE Hook_CreatePixelShader(' 'static bool ApplyInverseProjection('
$baselineSource = (& git show '98442ef:src/stereo_replay.inl') -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Pinned 98442ef baseline unavailable' }
$baselineHook = Extract-Source $baselineSource 'static HRESULT STDMETHODCALLTYPE Hook_CreatePixelShader(' 'static bool ApplyInverseProjection('

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools unavailable' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir = Join-Path $env:TEMP 'd2vr-shader-create-hook-test'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$helpers | Set-Content -LiteralPath (Join-Path $outDir 'actual_shader_helpers.inl') -Encoding utf8
$hook | Set-Content -LiteralPath (Join-Path $outDir 'actual_ps_create_hook.inl') -Encoding utf8

Push-Location $outDir
try {
    $test = Join-Path $repo 'test\shader_create_hook_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$outDir`" `"$test`" /Fe:shader-create-hook-test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Actual shader hook fixture build failed' }
    & .\shader-create-hook-test.exe @PrivateShaders
    if ($LASTEXITCODE -ne 0) { throw 'Actual shader hook fixture failed' }

    $baselineHook | Set-Content -LiteralPath (Join-Path $outDir 'actual_ps_create_hook.inl') -Encoding utf8
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /DD2VR_BASELINE=1 /I`"$outDir`" `"$test`" /Fe:shader-create-baseline-test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pinned baseline fixture build failed' }
    & .\shader-create-baseline-test.exe @PrivateShaders
    if ($LASTEXITCODE -ne 0) { throw 'Pinned baseline fixture failed' }
} finally {
    Pop-Location
}
