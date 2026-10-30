# Bootstrap script for Darkness2VR agent sessions
# Reports baseline, branch, and active handoffs. Supports -Role and -Task.

param(
    [string]$RepoPath = "",
    [ValidateSet("MAIN","SIDE")]
    [string]$Role = "",
    [string]$Task = ""
)

# Resolve repo location dynamically
if (-not $RepoPath) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $RepoPath = Resolve-Path (Join-Path $scriptDir "..\..") | Select-Object -ExpandProperty Path
}

Write-Host "=== Darkness2VR Agent Bootstrap ===" -ForegroundColor Cyan
Write-Host ""

Push-Location $RepoPath
try {
    $branch = git branch --show-current 2>&1
    $head = git rev-parse --short HEAD 2>&1
    $originHead = git rev-parse --short origin/HEAD 2>&1
    $mainHead = git rev-parse --short origin/main 2>&1
    
    Write-Host "Repo: $RepoPath" -ForegroundColor White
    Write-Host "Branch: $branch" -ForegroundColor Green
    Write-Host "HEAD: $head" -ForegroundColor Green
    Write-Host "origin/HEAD: $originHead" -ForegroundColor Gray
    Write-Host "origin/main: $mainHead" -ForegroundColor Gray
    if ($Role) { Write-Host "Role: $Role" -ForegroundColor $(if($Role -eq "MAIN"){"Yellow"}else{"Cyan"}) }
    if ($Task) { Write-Host "Task: $Task" -ForegroundColor Magenta }
    Write-Host ""
    
    # Baseline
    Write-Host "=== Baseline ===" -ForegroundColor Cyan
    if (Test-Path "docs\agent-workflow\BASELINE.md") {
        $baseline = Get-Content "docs\agent-workflow\BASELINE.md"
        $baseline | Select-String -Pattern "^(## Current|Repo HEAD|Latest MAIN|Integration branch|Accepted Steam|Installed DLL SHA256|Verified source commit|Branch \(current\)|Game)" | ForEach-Object { Write-Host $_.Line }
    } else {
        Write-Host "  BASELINE.md not found" -ForegroundColor Red
    }
    Write-Host ""
    
    # Active handoffs
    Write-Host "=== Active Task Handoffs ===" -ForegroundColor Cyan
    if (-not (Test-Path "docs\agent-workflow\handoffs")) {
        New-Item -ItemType Directory -Force -Path "docs\agent-workflow\handoffs" | Out-Null
    }
    $handoffs = Get-ChildItem "docs\agent-workflow\handoffs\*.md" -ErrorAction SilentlyContinue
    if ($handoffs) {
        foreach ($h in $handoffs) {
            Write-Host "  $($h.Name)" -ForegroundColor Yellow
            Get-Content $h.FullName -First 8 | Where-Object { $_ -match "^#|Date|Agent|Branch|Status|Goal|Role" } | ForEach-Object { Write-Host "    $_" }
        }
    } else {
        Write-Host "  None (create HANDOFF-<task-id>-<branch>.md from HANDOFF-TEMPLATE.md)" -ForegroundColor Gray
    }
    Write-Host ""
    
    # Failed approaches
    Write-Host "=== Failed Approaches (check before trying) ===" -ForegroundColor Cyan
    if (Test-Path "docs\agent-workflow\FAILED-APPROACHES.md") {
        Get-Content "docs\agent-workflow\FAILED-APPROACHES.md" | Select-String -Pattern "^###" | ForEach-Object { Write-Host "  $($_.Line)" }
    }
    Write-Host ""
    
    # Restrictions
    Write-Host "=== Access Restrictions ===" -ForegroundColor Cyan
    Write-Host "  MAIN ONLY: Steam launch, deployment, runtime testing, game install" -ForegroundColor Yellow
    Write-Host "  SIDE ONLY: Offline (build, analyze, test locally). NEVER touch Steam/game files" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "=== Quick Start ===" -ForegroundColor Cyan
    Write-Host "1. Read AGENTS.md" -ForegroundColor White
    Write-Host "2. Read BASELINE.md (check accepted Steam binary + verified source commit)" -ForegroundColor White
    Write-Host "3. Read/create your per-task handoff in handoffs/" -ForegroundColor White
    Write-Host "4. Check FAILED-APPROACHES.md" -ForegroundColor White
    Write-Host "5. Follow WORKFLOW.md" -ForegroundColor White
    
} finally {
    Pop-Location
}
