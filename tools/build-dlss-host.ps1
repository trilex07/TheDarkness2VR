# Build the 64-bit DLSS 4.5 / FSR helper (d2vr_dlss_host64.exe) into build\dlss_host\
# alongside nvngx_dlss.dll, the FidelityFX DLLs, and their notices.
#
# Prefers the local DLSS 4.5 runtime (v310.9.1.0 from DLSS Swapper if present on disk),
# falling back to third_party\ngx\lib\Windows_x86_64\rel\nvngx_dlss.dll (v310.7.0.0).
# Runs tools\fetch-ngx.ps1 and tools\fetch-ffx.ps1 first if third_party\ngx or third_party\ffx is absent.
$ErrorActionPreference = "Stop"
$repo  = Split-Path -Parent $PSScriptRoot
$ngx   = Join-Path $repo "third_party\ngx"
$ffx   = Join-Path $repo "third_party\ffx"
$out   = Join-Path $repo "build\dlss_host"
$src   = Join-Path $repo "src\tools\dlss_host\dlss_host.cpp"
$inc1  = Join-Path $repo "src"
$inc2  = Join-Path $ngx  "include"
$inc3  = Join-Path $ffx  "Kits\FidelityFX\api\include"
$inc4  = Join-Path $ffx  "Kits\FidelityFX\upscalers\include"
$lib   = Join-Path $ngx  "lib\Windows_x86_64\x64\nvsdk_ngx_d.lib"
$dllSdk = Join-Path $ngx "lib\Windows_x86_64\rel\nvngx_dlss.dll"
$ffxBin = Join-Path $ffx "Kits\FidelityFX\signedbin"

if (-not (Test-Path $lib)) {
    & (Join-Path $PSScriptRoot "fetch-ngx.ps1")
}
if (-not (Test-Path (Join-Path $ffxBin "amd_fidelityfx_loader_dx12.dll"))) {
    & (Join-Path $PSScriptRoot "fetch-ffx.ps1")
}

# Prefer DLSS 4.5 (v310.9.1.0) if available on the host machine
$dlss45Candidate = "C:\Users\Trilex\Desktop\DLSS.Swapper-1.2.4.0-portable\StoredData\dlls\dlss\dlss_v310.9.1.0_A0DAC8AB01C00F56A1DE1BC383F4147C\nvngx_dlss.dll"
$dll = if (Test-Path $dlss45Candidate) { $dlss45Candidate } else { $dllSdk }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products Microsoft.VisualStudio.Product.BuildTools `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    $vs = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $vs) { throw "Visual Studio C++ x64 build tools not found" }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $out "d2vr_dlss_host64.exe"
$obj = Join-Path $out "dlss_host.obj"

$cmd = "`"$vcvars`" >nul && cl.exe /nologo /std:c++17 /EHsc /O2 /MD /W3 /DUNICODE /D_UNICODE /D_WINDOWS " +
       "/I`"$inc1`" /I`"$inc2`" /I`"$inc3`" /I`"$inc4`" `"$src`" /Fo`"$obj`" /Fe`"$exe`" " +
       "/link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup `"$lib`" d3d12.lib dxgi.lib advapi32.lib user32.lib version.lib"
cmd.exe /c $cmd
if ($LASTEXITCODE -ne 0) { throw "cl.exe (x64) failed building d2vr_dlss_host64.exe" }
Remove-Item -ErrorAction SilentlyContinue $obj

Copy-Item $dll (Join-Path $out "nvngx_dlss.dll") -Force
Copy-Item (Join-Path $ffxBin "amd_fidelityfx_loader_dx12.dll")   (Join-Path $out "amd_fidelityfx_loader_dx12.dll") -Force
Copy-Item (Join-Path $ffxBin "amd_fidelityfx_upscaler_dx12.dll") (Join-Path $out "amd_fidelityfx_upscaler_dx12.dll") -Force
Copy-Item (Join-Path $ngx "LICENSE.txt") (Join-Path $out "NVIDIA-DLSS-LICENSE.txt") -Force
Copy-Item (Join-Path $ffx "LICENSE.md")  (Join-Path $out "AMD-FIDELITYFX-LICENSE.md") -Force
Copy-Item (Join-Path $repo "src\tools\dlss_host\NOTICE.md") (Join-Path $out "NOTICE.md") -Force

$ver = (Get-Item (Join-Path $out "nvngx_dlss.dll")).VersionInfo.FileVersion
Write-Host "build-dlss-host: built $exe with nvngx_dlss.dll ($ver) + AMD FidelityFX 2.3.0 / FSR 4.1.1"
