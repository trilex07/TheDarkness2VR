# =============================================================================
# prepare_hires_textures.ps1 — The Darkness II Hi-Res Texture Pipeline (SIDE)
#
# Purpose: Automate extraction + sorting of weapon/tentacle/enemy textures
# from Darkness II game packages (toc/cache) via QuickBMS, preparing a
# source folder structure that MAIN can feed into any AI upscaler
# (Real-ESRGAN, waifu2x, Topaz Gigapixel, etc.) to produce 2K/4K/8K textures.
#
# ROLE: SIDE prepares this script; MAIN runs it on his own machine
# (Steam exclusive, game-dir access). SIDE never installs AI, never touches
# the installed game, never launches Steam.
#
# Usage examples:
#   .\prepare_hires_textures.ps1 -Mode Extract -GameDir "C:\...\Darkness II"
#   .\prepare_hires_textures.ps1 -Mode Sort   -SourceDir "C:\extracted\output"
#   .\prepare_hires_textures.ps1 -Mode Prepare -SourceDir "C:\extracted\output" -DestDir ".\extracted_textures"
# =============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("Extract","Sort","Prepare")]
    [string]$Mode,

    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Darkness II",
    [string]$QuickBMS  = "$PSScriptRoot\..\quickbms.zip",       # extracted quickbms.exe lives next to this script
    [string]$BmsScript = "$PSScriptRoot\darkness2.bms",
    [string]$SourceDir,   # folder with quickbms output (for Sort/Prepare)
    [string]$DestDir  = "$PSScriptRoot\..\extracted_textures"   # repo-relative destination
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $PSScriptRoot

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Write-Log([string]$msg) { Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $msg) }

function Get-QuickBmsExe() {
    $candidates = @(
        "$env:TEMP\opencode\quickbms_extract\quickbms.exe",
        Join-Path $GameDir "Tools\quickbms.exe",
        "C:\quickbms\quickbms.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    throw "quickbms.exe not found. Extract quickbms.zip to a known location and pass -QuickBMS, or set GameDir correctly."
}

# Category keywords (file name substring -> destination category)
$Categories = @{
    "weapons"  = @("weapon","gun","pistol","shotgun","rifle","smg","sniper","ammo","spike","hellpistol","hellrifle","hellsmg","assault","burst");
    "tentacles"= @("tentacle","demonarm","stinger","secondarytentacle");
    "enemies"  = @("darkling","demon","enemy","boss","creature","monster","winged","cemetery","summon","suicide","gargoyle","mpdarknessboss");
}

function Classify-Texture([string]$name) {
    $lower = $name.ToLower()
    foreach ($cat in $Categories.Keys) {
        foreach ($kw in $Categories[$cat]) {
            if ($lower -like "*$kw*") { return $cat }
        }
    }
    return "environment"
}

# ---------------------------------------------------------------------------
# Mode: Extract — run QuickBMS on B.Texture, H.Texture, F.Texture packages
# ---------------------------------------------------------------------------
if ($Mode -eq "Extract") {
    if (-not (Test-Path $GameDir)) { throw "GameDir not found: $GameDir" }
    $qbms = Get-QuickBmsExe
    $work = Join-Path $env:TEMP "darkness_texture_extract_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
    $out  = Join-Path $work "output"
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    New-Item -ItemType Directory -Path $out  -Force | Out-Null

    $packages = @("B.Texture","H.Texture","F.Texture")
    foreach ($pkg in $packages) {
        $toc = Join-Path $GameDir "Cache.Windows\$pkg.toc"
        $cch = Join-Path $GameDir "Cache.Windows\$pkg.cache"
        if (-not (Test-Path $toc) -or -not (Test-Path $cch)) {
            Write-Log "SKIP $pkg (toc/cache missing)"; continue
        }
        Write-Log "Extracting $pkg ..."
        Copy-Item $toc "$work\toc.toc" -Force
        Copy-Item $cch "$work\toc.cache" -Force
        & $qbms $BmsScript "toc" $out > "$work\qbms_$pkg.log" 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Log "WARNING: quickbms exited $LASTEXITCODE for $pkg — see $work\qbms_$pkg.log"
        }
    }
    Write-Log "Extraction done. Output: $out"
    Write-Log "Next: .\prepare_hires_textures.ps1 -Mode Sort -SourceDir `"$out`""
    exit 0
}

# ---------------------------------------------------------------------------
# Mode: Sort — classify extracted PNGs into weapons/tentacles/enemies/environment
# ---------------------------------------------------------------------------
if ($Mode -eq "Sort") {
    if (-not $SourceDir) { throw "-SourceDir required for Sort mode" }
    if (-not (Test-Path $SourceDir)) { throw "SourceDir not found: $SourceDir" }
    $dest = Join-Path $DestDir "source"
    $counts = @{}
    Get-ChildItem $SourceDir -Recurse -Filter "*.png" | ForEach-Object {
        $cat = Classify-Texture $_.Name
        $catDir = Join-Path $dest $cat
        New-Item -ItemType Directory -Path $catDir -Force | Out-Null
        Copy-Item $_.FullName "$catDir\$($_.Name)" -Force
        $counts[$cat] = ($counts[$cat] + 1)
    }
    Write-Log "Sort done. Destination: $dest"
    foreach ($c in $counts.Keys) { Write-Log "  $c : $($counts[$c]) files" }
    Write-Log "Next: .\prepare_hires_textures.ps1 -Mode Prepare -SourceDir `"$SourceDir`" -DestDir `"$DestDir`""
    exit 0
}

# ---------------------------------------------------------------------------
# Mode: Prepare — create hires target structure + copy sources + manifest
# ---------------------------------------------------------------------------
if ($Mode -eq "Prepare") {
    if (-not $SourceDir) { throw "-SourceDir required for Prepare mode" }
    if (-not (Test-Path $SourceDir)) { throw "SourceDir not found: $SourceDir" }

    $resolutions = @("2k","4k","8k")
    $categories  = @("weapons","tentacles","enemies")

    # 1. Create target structure
    foreach ($res in $resolutions) {
        foreach ($cat in $categories) {
            $d = Join-Path $DestDir "$res\$cat"
            New-Item -ItemType Directory -Path $d -Force | Out-Null
        }
    }

    # 2. Copy source textures into 2k/source (MAIN upscales from here)
    $srcRoot = Join-Path $SourceDir
    # If a previous Sort run placed files under $DestDir\source, use that:
    $sortedRoot = Join-Path $DestDir "source"
    if (Test-Path $sortedRoot) { $srcRoot = $sortedRoot }

    $manifest = @()
    foreach ($cat in $categories) {
        $catSrc = Join-Path $srcRoot $cat
        if (-not (Test-Path $catSrc)) { continue }
        Get-ChildItem $catSrc -Filter "*.png" | ForEach-Object {
            $dst = Join-Path (Join-Path $DestDir "2k\source") $cat
            New-Item -ItemType Directory -Path $dst -Force | Out-Null
            Copy-Item $_.FullName "$dst\$($_.Name)" -Force
            $manifest += [pscustomobject]@{
                category = $cat
                source   = $_.FullName
                name     = $_.Name
                target2k = Join-Path (Join-Path $DestDir "2k") $cat
                target4k = Join-Path (Join-Path $DestDir "4k") $cat
                target8k = Join-Path (Join-Path $DestDir "8k") $cat
            }
        }
    }

    # 3. Write manifest CSV for MAIN to feed into AI batch upscaler
    $manifestPath = Join-Path $DestDir "upscale_manifest.csv"
    $manifest | Export-Csv -Path $manifestPath -NoTypeInformation -Encoding UTF8

    Write-Log "Prepare done. Destination root: $DestDir"
    Write-Log "  Source textures:  2k/source/{weapons,tentacles,enemies}/"
    Write-Log "  Target dirs:      {2k,4k,8k}/{weapons,tentacles,enemies}/"
    Write-Log "  Manifest:         $manifestPath ($($manifest.Count) files)"
    Write-Log ""
    Write-Log "MAIN: run your AI upscaler on 2k/source/*.png, writing results to {2k,4k,8k}/{category}/"
    Write-Log "MAIN: then install the upscaled DDS/PNG files into the game dir (Steam exclusive)."
    exit 0
}
