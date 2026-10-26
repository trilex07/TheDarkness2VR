param([switch]$ConstantOwnership, [switch]$Backend)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -property installationPath
$vc = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
if ($ConstantOwnership -and $Backend) { throw 'Choose one fixture' }
$out = if ($Backend) { Join-Path $env:TEMP 'd2vr-system9on12-test' } elseif ($ConstantOwnership) { Join-Path $env:TEMP 'd2vr-constant-ownership-test' } else { $here }
New-Item -ItemType Directory -Path $out -Force | Out-Null
$source = if ($Backend) { 'backend_test.cpp' } elseif ($ConstantOwnership) { 'constant_ownership_test.cpp' } else { 'stereo_harness.cpp' }
$target = if ($Backend) { 'backend_test.exe' } elseif ($ConstantOwnership) { 'constant_ownership_test.exe' } else { 'stereo_harness.exe' }
Push-Location $out
try {
    $sourcePath = Join-Path $here $source
    $libs = if ($Backend) { 'user32.lib d3d11.lib dxgi.lib d3dcompiler.lib' } else { 'user32.lib' }
    $extra=''
    if($Backend) {
        $mh=Join-Path (Split-Path -Parent (Split-Path -Parent $here)) 'third_party\minhook\src'
        $extra = '"' + (Join-Path $mh 'buffer.c') + '" "' + (Join-Path $mh 'hook.c') + '" "' + (Join-Path $mh 'trampoline.c') + '" "' + (Join-Path $mh 'hde\hde32.c') + '"'
    }
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 `"$sourcePath`" $extra /Fe:$target /link $libs"
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    if($Backend) {
        foreach($mode in @('native','on12')) {
            & (Join-Path $out $target) $mode
            if($LASTEXITCODE -ne 0) {throw "backend fixture failed: $mode"}
        }
    } else {
        & (Join-Path $out $target)
        if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
    }
} finally { Pop-Location }
