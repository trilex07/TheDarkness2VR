# Backup + deploy research d3d9.dll with SHA256 verification.
param(
    [switch]$Install,
    [switch]$DllOnly,
    [switch]$SkipIni,
    [string]$GameDir,
    [string]$RepoRoot = (Split-Path $PSScriptRoot -Parent)
)

$ErrorActionPreference = 'Stop'
if (-not $GameDir) {
    $game = & (Join-Path $PSScriptRoot 'lib\Find-Darkness2Game.ps1')
    $GameDir = $game.GameDir
}

$srcDll = Join-Path $RepoRoot 'd3d9.dll'
if (-not (Test-Path $srcDll)) {
    & (Join-Path $RepoRoot 'build.ps1')
    if (-not (Test-Path $srcDll)) { throw "Missing build output: $srcDll" }
}

$dstDll = Join-Path $GameDir 'd3d9.dll'
$backupDir = Join-Path $env:LOCALAPPDATA 'Darkness2VR\backups'
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDll = Join-Path $backupDir "d3d9-$stamp.dll"

function Get-Sha256($path) {
    if (-not (Test-Path $path)) { return $null }
    return (Get-FileHash -Algorithm SHA256 -Path $path).Hash
}

$origSha = Get-Sha256 $dstDll
$researchSha = Get-Sha256 $srcDll

Write-Host "Game: $GameDir"
Write-Host "Research DLL: $srcDll"
Write-Host "  SHA256: $researchSha"

if (Test-Path $dstDll) {
    Write-Host "Original DLL: $dstDll"
    Write-Host "  SHA256: $origSha"
    Copy-Item $dstDll $backupDll -Force
    Write-Host "Backup: $backupDll"
} else {
    Write-Host "Original DLL: (none in game dir)"
}

if ($Install) {
    if (Get-Process DarknessII -ErrorAction SilentlyContinue) {
        throw 'Darkness II is running. Close the game before deploy.'
    }
    Copy-Item $srcDll $dstDll -Force
    $deployedSha = Get-Sha256 $dstDll
    if ($deployedSha -ne $researchSha) { throw "Deploy verify failed: $deployedSha != $researchSha" }
    Write-Host 'Deployed OK - SHA256 match verified.'
}

if ($SkipIni -or $DllOnly) {
    return [pscustomobject]@{
        GameDir = $GameDir; OriginalSha = $origSha; ResearchSha = $researchSha
        BackupPath = $(if (Test-Path $backupDll) { $backupDll } else { $null }); LocalIni = $null
    }
}

# Ensure trace-friendly local config (does not overwrite entire ini if present).
$localIni = Join-Path $env:LOCALAPPDATA 'Darkness2VR\Darkness2VR.ini'
New-Item -ItemType Directory -Force -Path (Split-Path $localIni) | Out-Null
if (-not (Test-Path $localIni)) {
    Copy-Item (Join-Path $RepoRoot 'Darkness2VR.ini') $localIni -ErrorAction SilentlyContinue
}
$content = if (Test-Path $localIni) { Get-Content $localIni -Raw } else { '[Render]' + [Environment]::NewLine }
$lines = @(
    'RenderPipelineTrace=1',
    'FrameBoundaryPolicy=2',
    'SubmitMode=0',
    'StereoMode=0',
    'StartupMode=1',
    'DisableOpenXR=1',
    'HookMask=2'
)
foreach ($line in $lines) {
    $key = ($line -split '=')[0]
    $pat = '(?m)^' + [regex]::Escape($key) + '=.*'
    if ($content -match ('(?m)^' + [regex]::Escape($key) + '=')) {
        $content = [regex]::Replace($content, $pat, $line)
    } else {
        if ($content -notmatch '\[Render\]') { $content += [Environment]::NewLine + '[Render]' + [Environment]::NewLine }
        $content += $line + [Environment]::NewLine
    }
}
Set-Content -Path $localIni -Value $content -NoNewline

function Update-IniKeys($iniPath) {
    if (-not (Test-Path $iniPath)) { return }
    $c = Get-Content $iniPath -Raw
    foreach ($line in $lines) {
        $key = ($line -split '=')[0]
        $pat = '(?m)^' + [regex]::Escape($key) + '=.*'
        if ($c -match ('(?m)^' + [regex]::Escape($key) + '=')) {
            $c = [regex]::Replace($c, $pat, $line)
        } else {
            if ($c -notmatch '\[Render\]') { $c += [Environment]::NewLine + '[Render]' + [Environment]::NewLine }
            $c += $line + [Environment]::NewLine
        }
    }
    Set-Content -Path $iniPath -Value $c -NoNewline
}
$gameIni = Join-Path $GameDir 'Darkness2VR.ini'
if (Test-Path $gameIni) {
    Update-IniKeys $gameIni
    # Keys must live under [Render]; append if deploy regex missed orphaned keys.
    $raw = Get-Content $gameIni -Raw
    if ($raw -notmatch '(?m)^\[Render\][\s\S]*?RenderPipelineTrace=') {
        $raw = $raw -replace '(?m)^(\[Render\]\r?\n)', "`$1RenderPipelineTrace=1$([Environment]::NewLine)FrameBoundaryPolicy=2$([Environment]::NewLine)StartupMode=1$([Environment]::NewLine)DisableOpenXR=1$([Environment]::NewLine)"
        Set-Content -Path $gameIni -Value $raw -NoNewline
    }
}

return [pscustomobject]@{
    GameDir      = $GameDir
    OriginalSha  = $origSha
    ResearchSha  = $researchSha
    BackupPath   = $(if (Test-Path $backupDll) { $backupDll } else { $null })
    LocalIni     = $localIni
}
