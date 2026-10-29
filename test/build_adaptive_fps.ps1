$ErrorActionPreference='Stop'
$out=Join-Path $env:TEMP ('d2vr-adaptive-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
function Invoke-Regression([string]$path) {
    for($attempt=0;$attempt -le 20;++$attempt) {
        try { & $path; $runExit=$LASTEXITCODE; break }
        catch {
            $sharing=$false
            for($cause=$_.Exception;$null -ne $cause;$cause=$cause.InnerException) {
                if($cause -is [ComponentModel.Win32Exception] -and $cause.NativeErrorCode -eq 32){$sharing=$true}
            }
            if(!$sharing -or $attempt -eq 20){throw}
            if(!$attempt){Write-Host 'Test EXE has a sharing lock; retrying the same file for at most 40 seconds.'}
            Start-Sleep -Seconds 2
        }
    }
    if($runExit -ne 0){throw "Regression exited with $runExit : $path"}
}
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'adaptive_fps_test.cpp'
$exe=Join-Path $out 'd2vr-adaptive-fps.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fo:`"$out\d2vr-adaptive-fps.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Adaptive FPS compile failed'}
Invoke-Regression $exe
$hostSource=Get-Content (Join-Path $PSScriptRoot '../src/d3d9.cpp') -Raw
function Extract([string]$signature,[bool]$type=$false) {
    $start=$hostSource.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$hostSource.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $hostSource.Length;++$i){
        if($hostSource[$i] -eq '{'){++$depth}
        if($hostSource[$i] -eq '}'){--$depth;if(!$depth){return $hostSource.Substring($start,$i-$start+1)+$(if($type){';'}else{''})}}
    }
    throw "Unclosed $signature"
}
$actual=(Extract 'static uint64_t BeginAdaptivePresentation(')+"`r`n"+
    (Extract 'static void EndAdaptivePresentation(')+"`r`n"+
    (Extract 'class AdaptivePresentationScope {' $true)+"`r`n"+
    (Extract 'static void AdaptiveStereoSubmitted(')
[IO.File]::WriteAllText((Join-Path $out 'adaptive_actual.inl'),$actual,[Text.Encoding]::UTF8)
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$source=Join-Path $PSScriptRoot 'adaptive_fps_integration_test.cpp'
$exe=Join-Path $out 'd2vr-adaptive-integration.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 /I`"$out`" /I`"$sdk/include`" `"$source`" /Fo:`"$out\d2vr-adaptive-integration.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Adaptive host integration compile failed'}
Invoke-Regression $exe
