# Toggle Steam in-game overlay (global). Backs up and restores prior DWORD EnableOverlay.
param(
    [ValidateSet('On','Off','Restore')]
    [string]$State = 'On',
    [string]$BackupPath = (Join-Path $env:LOCALAPPDATA 'Darkness2VR\steam-overlay-backup.json')
)

$regPath = 'HKCU:\Software\Valve\Steam'
$name = 'EnableOverlay'
function Save-Backup {
    $cur = Get-ItemProperty -Path $regPath -Name $name -ErrorAction SilentlyContinue
    if ($cur) {
        @{ EnableOverlay = $cur.EnableOverlay } | ConvertTo-Json | Set-Content $BackupPath
    } else {
        @{ EnableOverlay = $null } | ConvertTo-Json | Set-Content $BackupPath
    }
}
switch ($State) {
    'Off' {
        Save-Backup
        New-Item -Path $regPath -Force | Out-Null
        Set-ItemProperty -Path $regPath -Name $name -Type DWord -Value 0
        Write-Host 'Steam overlay: OFF (global EnableOverlay=0). Restart Steam if overlay still loads.'
    }
    'On' {
        New-Item -Path $regPath -Force | Out-Null
        Set-ItemProperty -Path $regPath -Name $name -Type DWord -Value 1
        Write-Host 'Steam overlay: ON (global EnableOverlay=1)'
    }
    'Restore' {
        if (-not (Test-Path $BackupPath)) { Write-Host 'No overlay backup'; return }
        $b = Get-Content $BackupPath | ConvertFrom-Json
        New-Item -Path $regPath -Force | Out-Null
        if ($null -eq $b.EnableOverlay) {
            Remove-ItemProperty -Path $regPath -Name $name -ErrorAction SilentlyContinue
        } else {
            Set-ItemProperty -Path $regPath -Name $name -Type DWord -Value ([int]$b.EnableOverlay)
        }
        Write-Host 'Steam overlay: restored from backup'
    }
}
