$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP ('d2vr-swapchain-write-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$source=Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw
$state=[regex]::Match($source,'(?s)struct SwapchainWriteState \{.*?\r?\n\};\r?\nstatic SwapchainWriteState g_scWrite\[2\];')
$write=[regex]::Match($source,'(?s)static bool WriteTextureToSwapchain\(.*?\r?\n\}')
if(!$state.Success -or !$write.Success){throw 'Actual swapchain source missing'}
if($source -notmatch 'if\(XR_FAILED\(xrDestroySwapchain\(g_swapchain\[e\]\)\)\)return false;' -or $source -notmatch 'g_scWrite\[e\]=\{\};'){throw 'Swapchain destruction reset missing'}
($state.Value+"`n"+$write.Value) | Set-Content -LiteralPath (Join-Path $out 'actual_swapchain_write.inl')
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:\Users\Trilex\openxr-sdk'}
$test=Join-Path $PSScriptRoot 'swapchain_write_test.cpp'
Push-Location $out
try{
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$sdk/include`" /I`"$out`" `"$test`" /Fe:swapchain_write_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Swapchain regression build failed'}
    & .\swapchain_write_test.exe
    if($LASTEXITCODE -ne 0){throw 'Swapchain regression failed'}
}finally{Pop-Location}
