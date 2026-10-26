$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Get-Content -LiteralPath (Join-Path $here '..\..\src\d3d9.cpp') -Raw
function Get-FunctionBody([string]$signature, [string]$text = $source) {
    $start = $text.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing function: $signature" }
    $brace = $text.IndexOf('{', $start)
    $depth = 0
    for ($i = $brace; $i -lt $text.Length; ++$i) {
        if ($text[$i] -eq '{') { ++$depth }
        if ($text[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $text.Substring($start, $i-$start+1) }
        }
    }
    throw "Unclosed function: $signature"
}
# These functions contain no braces inside string literals. Compile their
# actual bodies, rather than a second implementation of the gate or forwarding.
$weaponSource = Get-Content -LiteralPath (Join-Path $here '..\..\src\native_weapon.inl') -Raw
$bodies = @(
    (Get-FunctionBody 'static void PublishXrPad('),
    (Get-FunctionBody 'static bool CopyXrPad('),
    (Get-FunctionBody 'static bool PadHasInput('),
    (Get-FunctionBody 'DWORD WINAPI Hook_XInputGetState('),
    (Get-FunctionBody 'void TryInstallXInputHook()'),
    (Get-FunctionBody 'static bool TryNativeWeaponMatrix(' $weaponSource)
)
$fixture = Join-Path $env:TEMP 'd2vr_xinput_bridge_under_test.inl'
[IO.File]::WriteAllText($fixture, ($bodies -join "`r`n"), [Text.Encoding]::UTF8)
$installer = Get-FunctionBody 'static void InstallDiagnosticTrackedGameplay()'
if ($installer -notmatch 'g_restoredTrackedGameplay=installed;[\s\S]*if\(installed\) TryInstallXInputHook\(\);') {
    throw 'Playable installer must install the bridge after native validation succeeds'
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $here
try {
    foreach ($variant in @('production','diagnostic')) {
        $define = if ($variant -eq 'diagnostic') { '/DD2VR_MATCHED_CAPTURE=1' } else { '' }
        $output = Join-Path $env:TEMP "d2vr-xinput-$variant.exe"
        $obj = Join-Path $env:TEMP "d2vr-xinput-$variant.obj"
        cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 $define /I`"$env:TEMP`" xinput_bridge_test.cpp /Fo:`"$obj`" /Fe:`"$output`""
        if ($LASTEXITCODE -ne 0) { throw "$variant build failed" }
        & $output
        if ($LASTEXITCODE -ne 0) { throw "$variant regression failed" }
    }
} finally { Pop-Location }
