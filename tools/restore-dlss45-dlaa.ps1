$ErrorActionPreference='Stop'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before restoring'}
$receipt=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
$game=[IO.Path]::GetFullPath($receipt.game_dir)
$hostDir=[IO.Path]::GetFullPath((Join-Path $game 'd2vr_dlss'))
if((Hash (Join-Path $game 'DarknessII.exe')) -ne $receipt.exe_sha256){throw 'EXE identity mismatch'}
if(@($receipt.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($entry in $receipt.before){
    if(!(Test-Path -LiteralPath $entry.backup -PathType Leaf) -or (Hash $entry.backup) -ne $entry.sha256){
        throw "Protected backup hash mismatch: $($entry.backup)"
    }
}
foreach($entry in $receipt.host){
    if([IO.Path]::GetFullPath((Split-Path -Parent $entry.target)) -ne $hostDir){throw 'Host target outside game d2vr_dlss'}
    if($entry.existed -and (!(Test-Path -LiteralPath $entry.backup -PathType Leaf) -or (Hash $entry.backup) -ne $entry.sha256)){
        throw "Host backup hash mismatch: $($entry.backup)"
    }
}
$preserve=Join-Path $PSScriptRoot ('before-restore-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
New-Item -ItemType Directory -Force -Path $preserve | Out-Null
$index=0
foreach($entry in $receipt.before){
    if(Test-Path -LiteralPath $entry.target -PathType Leaf){
        Copy-Item -LiteralPath $entry.target -Destination (Join-Path $preserve ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target)))
    }
    $index++
}
foreach($entry in $receipt.host){
    if(Test-Path -LiteralPath $entry.target -PathType Leaf){
        Copy-Item -LiteralPath $entry.target -Destination (Join-Path $preserve (Split-Path -Leaf $entry.target))
    }
}
foreach($entry in $receipt.before){
    Copy-Item -LiteralPath $entry.backup -Destination $entry.target -Force
    if((Hash $entry.target) -ne $entry.sha256){throw "Protected restore verification failed: $($entry.target)"}
}
foreach($entry in $receipt.host){
    if($entry.existed){
        Copy-Item -LiteralPath $entry.backup -Destination $entry.target -Force
        if((Hash $entry.target) -ne $entry.sha256){throw "Host restore verification failed: $($entry.target)"}
    } elseif(Test-Path -LiteralPath $entry.target -PathType Leaf){
        Remove-Item -LiteralPath $entry.target -Force
    }
}
if(Test-Path -LiteralPath $hostDir -PathType Container){
    if(@(Get-ChildItem -LiteralPath $hostDir -Force).Count -eq 0){Remove-Item -LiteralPath $hostDir}
}
Write-Output 'RESTORED: nine protected targets and prior DLSS host state; hashes verified. Game not launched.'
