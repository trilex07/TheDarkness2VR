# Resolve The Darkness II (Steam AppID 67370) install without hardcoded paths.
param([int]$AppId = 67370)

$ErrorActionPreference = 'Stop'

function Get-SteamLibraryRoots {
    $roots = [System.Collections.Generic.List[string]]::new()
    try {
        $steamPath = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath
        if ($steamPath -and (Test-Path $steamPath)) { [void]$roots.Add($steamPath) }
    } catch {}
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Steam'),
        (Join-Path $env:ProgramFiles 'Steam')
    )
    foreach ($c in $candidates) {
        if ((Test-Path $c) -and ($roots -notcontains $c)) { [void]$roots.Add($c) }
    }
    foreach ($base in @($roots)) {
        $vdf = Join-Path $base 'steamapps\libraryfolders.vdf'
        if (-not (Test-Path $vdf)) { continue }
        $lines = Get-Content $vdf
        foreach ($line in $lines) {
            if ($line -match '^\s*"path"\s+"(.+)"\s*$') {
                $p = $Matches[1] -replace '\\\\', '\'
                if ((Test-Path $p) -and ($roots -notcontains $p)) { [void]$roots.Add($p) }
            }
        }
    }
    return $roots | Select-Object -Unique
}

function Get-GameFromManifest($libraryRoot, $appId) {
    $manifest = Join-Path $libraryRoot "steamapps\appmanifest_$appId.vdf"
    if (-not (Test-Path $manifest)) { return $null }
    $text = Get-Content $manifest -Raw
    if ($text -notmatch '"installdir"\s+"([^"]+)"') { return $null }
    $installDir = $Matches[1]
    $gameDir = Join-Path $libraryRoot "steamapps\common\$installDir"
    $exe = Join-Path $gameDir 'DarknessII.exe'
    if (-not (Test-Path $exe)) { return $null }
    return [pscustomobject]@{
        AppId        = $appId
        SteamRoot    = $libraryRoot
        InstallDir   = $installDir
        GameDir      = $gameDir
        ExePath      = $exe
        ManifestPath = $manifest
    }
}

$roots = Get-SteamLibraryRoots
foreach ($r in $roots) {
    $hit = Get-GameFromManifest $r $AppId
    if ($hit) { return $hit }
}
# Common default install even if libraryfolders.vdf parsing fails.
$default = Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\Darkness II\DarknessII.exe'
if (Test-Path $default) {
    return [pscustomobject]@{
        AppId        = $AppId
        SteamRoot    = Join-Path ${env:ProgramFiles(x86)} 'Steam'
        InstallDir   = 'Darkness II'
        GameDir      = Split-Path $default -Parent
        ExePath      = $default
        ManifestPath = Join-Path ${env:ProgramFiles(x86)} "Steam\steamapps\appmanifest_$AppId.vdf"
        Discovery    = 'default-path'
    }
}

# Fallback: shallow search on common drives (bounded depth).
$drives = Get-PSDrive -PSProvider FileSystem | Select-Object -ExpandProperty Root
foreach ($d in $drives) {
    try {
        $found = Get-ChildItem -Path $d -Filter 'DarknessII.exe' -Recurse -Depth 7 -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'Darkness II|DarknessII|darkness ii' } |
            Select-Object -First 1
        if ($found) {
            return [pscustomobject]@{
                AppId        = $AppId
                SteamRoot    = $null
                InstallDir   = $null
                GameDir      = $found.Directory.FullName
                ExePath      = $found.FullName
                ManifestPath = $null
                Discovery    = 'filesystem-search'
            }
        }
    } catch {}
}

throw "Darkness II (AppID $AppId) not found. Checked $($roots.Count) Steam libraries."
