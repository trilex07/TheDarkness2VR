# Read-only session summary. No mkdir, fetch, build, install or Steam access.
param(
    [string]$RepoPath = '',
    [ValidateSet('MAIN','SIDE')][string]$Role = 'SIDE',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$Task
)
$ErrorActionPreference='Stop'
if(-not $RepoPath){$RepoPath=Join-Path $PSScriptRoot '../..'}
$RepoPath=(Resolve-Path -LiteralPath $RepoPath).Path
function Read-Git([string[]]$Arguments,[switch]$Optional) {
    $value=& git --no-optional-locks -C $RepoPath @Arguments
    if($LASTEXITCODE -ne 0){
        if($Optional){return '<unavailable>'}
        throw ('Git read failed: '+($Arguments -join ' '))
    }
    return ($value -join "`n").Trim()
}
$top=Read-Git -Arguments @('rev-parse','--show-toplevel')
if([IO.Path]::GetFullPath($top) -ne [IO.Path]::GetFullPath($RepoPath)){
    throw 'RepoPath must be the assigned worktree root'
}
$branch=Read-Git -Arguments @('branch','--show-current')
if(-not $branch){$branch='(detached HEAD)'}
$head=Read-Git -Arguments @('rev-parse','--short','HEAD')
$originHead=Read-Git -Arguments @('for-each-ref','--format=%(objectname:short)','refs/remotes/origin/HEAD')
if(-not $originHead){$originHead='<unavailable>'}
$mainHead=Read-Git -Arguments @('for-each-ref','--format=%(objectname:short)','refs/remotes/origin/main')
if(-not $mainHead){$mainHead='<unavailable>'}
Write-Host '=== Darkness2VR Agent Bootstrap (READ ONLY) ==='
Write-Host "Repo: $RepoPath"
Write-Host "Branch: $branch"
Write-Host "HEAD: $head"
Write-Host "origin/HEAD: $originHead (informational, not deployment proof)"
Write-Host "origin/main: $mainHead (informational, not deployment proof)"
Write-Host "Role: $Role"
if($Task){Write-Host "Task: $Task"}
Write-Host '=== Baseline (recorded snapshot; no live Steam access) ==='
$baseline=Join-Path $RepoPath 'docs/agent-workflow/BASELINE.md'
$fields=@('Accepted DLL SHA256','Accepted source commit','Installed DLL SHA256',
 'Installed INI SHA256','Installed local INI SHA256','Deployed DLL source commit',
 'Verified source commit','Integration branch','Latest MAIN runtime evidence commit','Acceptance status')
if(Test-Path -LiteralPath $baseline -PathType Leaf){
    foreach($line in (Get-Content -LiteralPath $baseline -Encoding UTF8)){
        if($line -match '^-\s+\*\*(?<key>[^*]+)\*\*:\s*(?<value>.+)$' -and $fields -contains $Matches.key){
            Write-Host ("  {0}: {1}" -f $Matches.key,$Matches.value)
        }
    }
}else{Write-Host '  BASELINE.md not found; do not assume a deployed source'}
Write-Host '=== Per-task handoffs ==='
$handoffDir=Join-Path $RepoPath 'docs/agent-workflow/handoffs'
if($Task){
    $handoff=Join-Path $handoffDir "HANDOFF-$Task.md"
    Write-Host "Task handoff: $handoff"
    if(Test-Path -LiteralPath $handoff -PathType Leaf){
        Get-Content -LiteralPath $handoff -Encoding UTF8 -First 14 |
            Where-Object {$_ -match '^#|^\*\*(Task ID|Date|Role|Branch|Status|Integration commit)\*\*:'} |
            ForEach-Object {Write-Host "  $_"}
    }else{Write-Host '  Not found; create manually from HANDOFF-TEMPLATE.md in this task worktree only'}
}elseif(Test-Path -LiteralPath $handoffDir -PathType Container){
    Get-ChildItem -LiteralPath $handoffDir -File -Filter 'HANDOFF-*.md' |
        Sort-Object Name | ForEach-Object {Write-Host "  $($_.Name)"}
}else{Write-Host '  No handoff directory; not created (read-only mode)'}
Write-Host '=== Access rules ==='
Write-Host 'MAIN: deployment/Steam/runtime testing only when explicitly authorized, with verified rollback.'
Write-Host 'SIDE: offline only; NEVER modify installed game/Steam files, launch Steam or run in-game tests.'
Write-Host 'Role selection grants no OS permissions; session restrictions and user instructions still apply.'
Write-Host '=== Next ==='
Write-Host 'Read AGENTS.md first, BASELINE.md, your HANDOFF-<task-id>.md and relevant FAILED-APPROACHES entries.'
Write-Host 'Keep actual Git branch inside the handoff; never append it to the filename.'
