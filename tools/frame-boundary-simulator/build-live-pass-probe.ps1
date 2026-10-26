$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$source=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
$begin=$source.IndexOf('HRESULT STDMETHODCALLTYPE Hook_SetVertexShaderConstantF(')
$end=$source.IndexOf('// ---------------------------------------------------------------- INTZ Depth Capture',$begin)
if($begin -lt 0 -or $end -lt 0){throw 'actual hook source boundaries not found'}
if(-not $source.Contains('g_livePassProbe=GetPrivateProfileIntA("Diagnostics","LivePassProbe",0,')) {throw 'explicit diagnostic startup option/default0 missing'}
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-live-pass-probe-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$source.Substring($begin,$end-$begin) | Set-Content -LiteralPath (Join-Path $outDir 'actual_live_vs_hook.inl') -Encoding utf8
Push-Location $outDir
try {
    $test=Join-Path $repo 'test\live_vs_hook_test.cpp'
    foreach($diagnostic in @($false,$true)) {
        $define=if($diagnostic){'/DD2VR_MATCHED_CAPTURE=1'}else{''}
        cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 $define /I`"$outDir`" `"$test`" /Fe:live-pass-test.exe"
        if($LASTEXITCODE -ne 0){throw 'actual live VS hook fixture build failed'}
        foreach($case in @('disabled','legacy','cap','stereo')) {
            & .\live-pass-test.exe $case
            if($LASTEXITCODE -ne 0){throw "actual hook case failed: $case diagnostic=$diagnostic"}
        }
    }
} finally {Pop-Location}
