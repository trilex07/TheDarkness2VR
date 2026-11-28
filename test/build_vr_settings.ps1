param([string]$OutputDir)
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
if(!$OutputDir){$OutputDir=Join-Path $env:TEMP ('d2vr-settings-'+[Guid]::NewGuid().ToString('N'))}
$OutputDir=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$hostSource=Get-Content -LiteralPath (Join-Path $here '../src/d3d9.cpp') -Raw
$inputStart=$hostSource.IndexOf('    // Menu input ownership is decided before triangle/recenter/gesture actions.')
$inputEnd=$hostSource.IndexOf('    // A native-confirmed character/retained-target hold',$inputStart)
if($inputStart -lt 0 -or $inputEnd -le $inputStart){throw 'Actual VR settings input routing missing'}
[IO.File]::WriteAllText((Join-Path $OutputDir 'actual_vr_settings_input.inl'),$hostSource.Substring($inputStart,$inputEnd-$inputStart))
$submitStart=$hostSource.IndexOf('    const uint32_t worldLayerCount=layerCount;')
$submitEnd=$hostSource.IndexOf('    AdaptiveStereoSubmitted(endR,havePixels,worldLayerCount);',$submitStart)
if($submitStart -lt 0 -or $submitEnd -le $submitStart){throw 'Actual VR settings submission missing'}
$submitEnd+='    AdaptiveStereoSubmitted(endR,havePixels,worldLayerCount);'.Length
[IO.File]::WriteAllText((Join-Path $OutputDir 'actual_vr_settings_submission.inl'),$hostSource.Substring($submitStart,$submitEnd-$submitStart))
$cinemaStart=$hostSource.IndexOf('static d2vr_cinema::Override CinemaOverride() {')
$cinemaEnd=$hostSource.IndexOf('#include "bink_playback.inl"',$cinemaStart)
if($cinemaStart -lt 0 -or $cinemaEnd -le $cinemaStart){throw 'Actual cinema controls missing'}
[IO.File]::WriteAllText((Join-Path $OutputDir 'actual_cinema_control.inl'),$hostSource.Substring($cinemaStart,$cinemaEnd-$cinemaStart))
$recenterStart=$hostSource.IndexOf('static void CaptureClickRecenterReference() {')
$recenterEnd=$hostSource.IndexOf('// Builds the per-eye transform parameters.', $recenterStart)
if($recenterStart -lt 0 -or $recenterEnd -le $recenterStart){throw 'Actual head recenter implementation missing'}
[IO.File]::WriteAllText((Join-Path $OutputDir 'actual_head_recenter.inl'),$hostSource.Substring($recenterStart,$recenterEnd-$recenterStart))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
Push-Location $OutputDir
try {
    foreach($name in @('head_recenter_test','cinema_persistent_test','vr_settings_test','vr_settings_renderer_test','vr_settings_input_host_test','vr_settings_submission_test','vr_gameplay_preferences_test','weapon_laser_test')) {
        $source=Join-Path $here ($name+'.cpp')
        if(!(Test-Path -LiteralPath $source)){throw "Missing $source"}
        cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk\include`" /I`"$OutputDir`" `"$source`" /Fe:$name.exe /link user32.lib gdi32.lib"
        if($LASTEXITCODE){throw "$name build failed"}
        & (Join-Path $OutputDir ($name+'.exe'))
        if($LASTEXITCODE){throw "$name regression failed"}
    }
} finally {Pop-Location}
