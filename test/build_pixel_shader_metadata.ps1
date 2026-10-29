param([switch]$ExpectLegacyStale)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-pixel-metadata'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
function Extract([string]$signature) {
    $start=$text.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$text.IndexOf('{',$start);$depth=0;$end=-1
    for($i=$brace;$i -lt $text.Length;++$i){
        if($text[$i] -eq '{'){++$depth}
        if($text[$i] -eq '}'){--$depth;if(!$depth){$end=$i;break}}
    }
    if($end -lt 0){throw "Unclosed $signature"}
    return $text.Substring($start,$end-$start+1)
}
$parts=@((Extract 'static int FindNamedRegister('),(Extract 'static int FindNamedMatrix('),
    (Extract 'static uint32_t HashBytes('),(Extract 'static size_t SafeShaderTokenCount('),
    (Extract 'static bool BuildUnfilteredFxaa('),(Extract 'static HRESULT STDMETHODCALLTYPE Hook_CreatePixelShader('))
if(-not $ExpectLegacyStale){
    $parts+=(Extract 'static void SetActivePixelShaderMetadata(')
    $parts+=(Extract 'static HRESULT STDMETHODCALLTYPE Hook_SetPixelShader(')
    $parts+=(Extract 'static HRESULT STDMETHODCALLTYPE Hook_SetVertexShader(')
    $replay=$text.Substring($text.IndexOf('static bool ReplayStream('))
    $cases=foreach($op in 'SetVertexShader','SetPixelShader') {
        $case=[regex]::Match($replay,"(?s)        case OP_${op}:.*?(?=        case |        default:)")
        if(-not $case.Success){throw "Missing replay shader case $op"};$case.Value
    }
    [IO.File]::WriteAllText((Join-Path $out 'actual_shader_replay.inl'),($cases -join "`r`n"))
    [IO.File]::WriteAllText((Join-Path $out 'actual_pixel_sync.inl'),
        (Extract 'static bool SyncActiveShader(IDirect3DDevice9* dev) {').Replace('IDirect3DDevice9* dev','FixtureDevice* dev'))
    [IO.File]::WriteAllText((Join-Path $out 'actual_pixel_light.inl'),
        (Extract 'if (g_lightPosShift && sp.transX'))
}
[IO.File]::WriteAllText((Join-Path $out 'actual_pixel_metadata.inl'),($parts -join "`r`n"))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'pixel_shader_metadata_test.cpp'
$define=if($ExpectLegacyStale){'/DD2VR_EXPECT_LEGACY_STALE=1'}else{''}
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 $define /I`"$out`" `"$source`" /Fe:pixel_metadata.exe"
    if($LASTEXITCODE -ne 0){throw 'Pixel metadata build failed'}
    & .\pixel_metadata.exe
    if($LASTEXITCODE -ne 0){throw 'Pixel metadata regression failed'}
} finally {Pop-Location}
