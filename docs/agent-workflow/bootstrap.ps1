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
    if ($Task) { 
        # Sanitize task for filename
        $safeTask = $Task -replace '[^a-zA-Z0-9_-]', '_'
        Write-Host "Task: $Task (safe: $safeTask)" -ForegroundColor Magenta 
    }
    Write-Host ""
    
    # Baseline - parse Markdown bullet fields
    Write-Host "=== Baseline ===" -ForegroundColor Cyan
    if (Test-Path "docs\agent-workflow\BASELINE.md") {
        $baseline = Get-Content "docs\agent-workflow\BASELINE.md"
        $patterns = @(
            'Installed DLL SHA256',
            'Installed INI SHA256',
            'Deployed DLL source commit',
            'Verified source commit',
            'Integration branch',
            'Latest MAIN',
            'Repo DLL SHA256',
            'Repo INI SHA256',
            'Current workflow branch'
        )
        foreach ($p in $patterns) {
            $line = $baseline | Where-Object { $_ -match "- \*\*$p\*\*:" -or $_ -match "- $p" } | Select-Object -First 1
            if ($line) { Write-Host "  $($line.Trim())" }
        }
    } else {
        Write-Host "  BASELINE.md not found" -ForegroundColor Red
    }
    Write-Host ""
    
    # Active handoffs - read only, no directory creation
    Write-Host "=== Active Task Handoffs ===" -ForegroundColor Cyan
    if (Test-Path "docs\agent-workflow\handoffs") {
        $handoffs = Get-ChildItem "docs\agent-workflow\handoffs\*.md" -ErrorAction SilentlyContinue
        if ($handoffs) {
            foreach ($h in $handoffs) {
                Write-Host "  $($h.Name)" -ForegroundColor Yellow
                Get-Content $h.FullName -First 8 | Where-Object { $_ -match "^#|Date|Agent|Branch|Status|Goal|Role" } | ForEach-Object { Write-Host "    $_" }
            }
        } else {
            Write-Host "  None (create HANDOFF-<task-id>-<branch>.md from HANDOFF-TEMPLATE.md)" -ForegroundColor Gray
        }
    } else {
        Write-Host "  handoffs/ directory does not exist (read-only mode - not created)" -ForegroundColor Gray
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
    Write-Host "2. Read BASELINE.md (check accepted Steam binary + deployed DLL source commit)" -ForegroundColor White
    Write-Host "3. Read/create your per-task handoff in handoffs/ (use Windows-safe filename)" -ForegroundColor White
    Write-Host "4. Check FAILED-APPROACHES.md" -ForegroundColor White
    Write-Host "5. Follow WORKFLOW.md" -ForegroundColor White
    
} finally {
    Pop-Location
}
