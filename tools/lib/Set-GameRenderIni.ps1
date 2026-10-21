# Set keys under [Render] in game Darkness2VR.ini (Windows INI safe).
param(
    [Parameter(Mandatory)]
    [string]$GameDir,
    [Parameter(Mandatory)]
    [hashtable]$Keys
)
$ini = Join-Path $GameDir 'Darkness2VR.ini'
if (-not (Test-Path $ini)) { throw "Missing $ini" }
$lines = [System.Collections.Generic.List[string]]::new()
$lines.AddRange([string[]](Get-Content $ini))
$renderIdx = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\[Render\]') { $renderIdx = $i; break }
}
if ($renderIdx -lt 0) { throw 'No [Render] section' }
$end = $lines.Count
for ($i = $renderIdx + 1; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\[') { $end = $i; break }
}
foreach ($key in $Keys.Keys) {
    $val = $Keys[$key]
    $line = "$key=$val"
    $found = $false
    for ($i = $renderIdx + 1; $i -lt $end; $i++) {
        if ($lines[$i] -match "^$([regex]::Escape($key))=") {
            $lines[$i] = $line
            $found = $true
            break
        }
    }
    if (-not $found) {
        $lines.Insert($renderIdx + 1, $line)
        $end++
    }
}
Set-Content -Path $ini -Value ($lines -join [Environment]::NewLine)
