$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$source=Get-Content (Join-Path $repo 'src\d3d9.cpp') -Raw
$runtime=Get-Content (Join-Path $repo 'src\image_present_runtime.inl') -Raw
function Body([string]$text,[string]$signature) {
    $start=$text.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing definition $signature"}
    $brace=$text.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $text.Length;++$i){
        if($text[$i] -eq '{'){++$depth}
        if($text[$i] -eq '}'){--$depth;if(!$depth){return $text.Substring($start,$i-$start+1)}}
    }
    throw "Unclosed definition $signature"
}
# Compile the actual callback, event lifecycle, boundary admission, and return
# bodies. Only the GPU VrFrame leaf and OpenXR runtime are deterministic fakes.
$parts=@((Body $source 'void PumpXR() {'),
    (Body $source 'static void ApplyBoundaryDecision(IDirect3DDevice9* dev, const d2vr_frame::Decision& d) {'),
    ((Body $runtime 'struct ImagePresentScope {')+';'),
    'static thread_local ImagePresentScope g_imagePresentScope;',
    (Body $runtime 'static void ImagePresentBefore('),
    (Body $runtime 'static void ImagePresentAfter('))
$begin=Body $source 'static void BeginTrackedFrame() {'
if(([regex]::Matches($begin,'\bPumpXR\(')).Count -ne 1){throw 'Ready path event ownership changed'}
$outDir=Join-Path $env:TEMP 'd2vr-xr-event-recovery'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $outDir 'xr_event_recovery_under_test.inl'),($parts -join "`r`n"),[Text.Encoding]::UTF8)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$fixture=Join-Path $here 'image_present_fixture.cpp'
$test=Join-Path $here 'xr_event_recovery_test.cpp'
$mh=Join-Path $repo 'third_party\minhook\src'
Push-Location $outDir
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /LD /EHsc /W3 /Od /MD /std:c++17 `"$fixture`" /Fe:fixture.dll /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'fixture build failed'}
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /DD2VR_MATCHED_CAPTURE=1 /I`"$sdk\include`" /I`"$outDir`" `"$test`" `"$mh\buffer.c`" `"$mh\hook.c`" `"$mh\trampoline.c`" `"$mh\hde\hde32.c`" /Fe:xr-recovery-test.exe /link user32.lib"
    if($LASTEXITCODE -ne 0){throw 'event recovery build failed'}
    & .\xr-recovery-test.exe (Join-Path $outDir 'fixture.dll')
    if($LASTEXITCODE -ne 0){throw 'event recovery regression failed'}
} finally {Pop-Location}
