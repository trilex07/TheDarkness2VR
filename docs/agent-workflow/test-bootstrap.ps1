# Offline workflow tests. All fixture writes stay in a unique TEMP directory.
$ErrorActionPreference='Stop'
$bootstrap=Join-Path $PSScriptRoot 'bootstrap.ps1'
$fixture=Join-Path $env:TEMP ('d2vr-bootstrap-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
& git init --quiet -b 'fixture/cpu-render' $fixture
if($LASTEXITCODE){throw 'Fixture init failed'}
& git -C $fixture -c user.name=Fixture -c user.email=fixture@invalid -c commit.gpgsign=false commit --allow-empty --quiet -m 'Synthetic workflow fixture'
if($LASTEXITCODE){throw 'Fixture commit failed'}
$workflow=Join-Path $fixture 'docs/agent-workflow'
New-Item -ItemType Directory -Path $workflow | Out-Null
@"
# Synthetic baseline
- **Accepted source commit**: accepted-fixture
- **Installed DLL SHA256**: INSTALLED_FIXTURE_SHA
- **Deployed DLL source commit**: deployed-fixture
- **Integration branch**: fixture/cpu-render
- **Acceptance status**: fixture-only
"@ | Set-Content -LiteralPath (Join-Path $workflow 'BASELINE.md') -Encoding UTF8
$checks=0
function Check([bool]$value,[string]$message){if(-not $value){throw $message};$script:checks++}
function Snapshot {
    $items=@(Get-ChildItem -LiteralPath $fixture -Force -Recurse | Sort-Object FullName | ForEach-Object {
        if($_.PSIsContainer){'D|'+$_.FullName}else{'F|'+$_.FullName+'|'+(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    return ($items -join "`n")
}
function Invoke-Bootstrap([string]$role,[string]$task) {
    $before=Snapshot
    $text=& $bootstrap -RepoPath $fixture -Role $role -Task $task 6>&1 | Out-String
    Check ($before -ceq (Snapshot)) 'Bootstrap modified fixture files, Git metadata or directories'
    return $text
}
foreach($role in 'MAIN','SIDE') {
    $output=Invoke-Bootstrap $role 'missing-task'
    Check ($output.Contains("Role: $role")) 'Incorrect role'
    Check ($output.Contains('Installed DLL SHA256: INSTALLED_FIXTURE_SHA')) 'Markdown bullet parsing failed'
    Check ($output.Contains('Deployed DLL source commit: deployed-fixture')) 'Baseline source parsing failed'
    Check ($output.Contains('Branch: fixture/cpu-render')) 'Actual branch with slash lost'
    Check ($output.Contains('origin/main: <unavailable>')) 'Missing optional remote treated as accepted baseline'
    Check ($output.Contains('HANDOFF-missing-task.md')) 'Unsafe/incorrect handoff filename'
}
Check (-not(Test-Path -LiteralPath (Join-Path $workflow 'handoffs'))) 'Read-only bootstrap created handoff directory'
$handoffs=Join-Path $workflow 'handoffs'
New-Item -ItemType Directory -Path $handoffs | Out-Null
foreach($task in 'alpha','beta') {
    "# Handoff: $task`n`n**Task ID**: $task`n**Branch**: fixture/cpu-render`n**Status**: ONLY_$task" |
        Set-Content -LiteralPath (Join-Path $handoffs "HANDOFF-$task.md") -Encoding UTF8
}
$a=Invoke-Bootstrap MAIN alpha
$b=Invoke-Bootstrap SIDE beta
Check ($a.Contains('ONLY_alpha') -and -not $a.Contains('ONLY_beta')) 'Alpha loaded another task handoff'
Check ($b.Contains('ONLY_beta') -and -not $b.Contains('ONLY_alpha')) 'Beta loaded another task handoff'
Check ($b.Contains('SIDE: offline only; NEVER modify installed game/Steam files')) 'SIDE Steam restriction missing'
foreach($bad in @('../escape','cpu/render','cpu:render','cpu\render','two words','trailing.','..','x*','',('x'*65))) {
    $before=Snapshot;$rejected=$false
    try { & $bootstrap -RepoPath $fixture -Role SIDE -Task $bad *> $null } catch {$rejected=$true}
    Check $rejected "Unsafe task ID accepted: [$bad]"
    Check ($before -ceq (Snapshot)) 'Rejected task wrote files'
}
$before=Snapshot;$rejected=$false
try { & $bootstrap -RepoPath $fixture -Role UNKNOWN -Task alpha *> $null } catch {$rejected=$true}
Check $rejected 'Unknown role accepted'
Check ($before -ceq (Snapshot)) 'Rejected role wrote files'
$entry=Get-Content (Join-Path $PSScriptRoot '../../AGENTS.md') -Raw
$workflowRules=Get-Content (Join-Path $PSScriptRoot 'WORKFLOW.md') -Raw
Check ($entry.Contains('Mandatory entry point')) 'Root entrypoint not mandatory'
Check ($entry.Contains('SIDE: offline only') -and $workflowRules.Contains('SIDE NEVER')) 'SIDE rules missing from mandatory docs'
$stale=Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.md' -Recurse | Select-String -Pattern 'HANDOFF-CURRENT|<task-id>-<branch>'
Check (-not $stale) 'Obsolete/shared branch-derived handoff convention remains'
Write-Host "PASS: $checks bootstrap checks; MAIN/SIDE read-only, baseline parsing, isolated task IDs, invalid Windows names, no Steam access."
Write-Host "Synthetic fixture retained: $fixture"
