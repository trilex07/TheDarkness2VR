# Bootstrap script for Darkness2VR agent sessions
# Reports baseline, branch, active handoffs, relevant failed approaches

param(
    [string]$RepoPath = "C:\Users\Trilex\Darkness2VR"
)

Write-Host "=== Darkness2VR Agent Bootstrap ===" -ForegroundColor Cyan
Write-Host ""

# Git info
Push-Location $RepoPath
try {
    $branch = git branch --show-current 2>&1
    $head = git rev-parse --short HEAD 2>&1
    Write-Host "Branch: $branch" -ForegroundColor Green
    Write-Host "HEAD: $head" -ForegroundColor Green
    Write-Host ""
    
    # Baseline
    Write-Host "=== Baseline ===" -ForegroundColor Cyan
    if (Test-Path "docs\agent-workflow\BASELINE.md") {
        Get-Content "docs\agent-workflow\BASELINE.md" | Select-String -Pattern "^(## Current|Repo HEAD|Installed DLL SHA256|Branch|Game)" | ForEach-Object { Write-Host $_.Line }
    }
    Write-Host ""
    
    # Active handoffs
    Write-Host "=== Active Handoffs ===" -ForegroundColor Cyan
    $handoffs = Get-ChildItem "docs\agent-workflow\handoffs\*.md" -ErrorAction SilentlyContinue
    if ($handoffs) {
        foreach ($h in $handoffs) {
            Write-Host "  $($h.Name)" -ForegroundColor Yellow
            $firstLine = Get-Content $h.FullName -First 5 | Where-Object { $_ -match "^#|Date|Agent|Branch|Status|Goal" }
            $firstLine | ForEach-Object { Write-Host "    $_" }
        }
    } else {
        Write-Host "  None (create from HANDOFF-TEMPLATE.md)" -ForegroundColor Gray
    }
    Write-Host ""
    
    # Failed approaches (summary)
    Write-Host "=== Failed Approaches (check before trying) ===" -ForegroundColor Cyan
    if (Test-Path "docs\agent-workflow\FAILED-APPROACHES.md") {
        Get-Content "docs\agent-workflow\FAILED-APPROACHES.md" | Select-String -Pattern "^###" | ForEach-Object { Write-Host "  $($_.Line)" }
    }
    Write-Host ""
    
    Write-Host "=== Quick Start ===" -ForegroundColor Cyan
    Write-Host "1. Read AGENTS.md" -ForegroundColor White
    Write-Host "2. Read BASELINE.md" -ForegroundColor White
    Write-Host "3. Read your task handoff from handoffs/" -ForegroundColor White
    Write-Host "4. Check FAILED-APPROACHES.md" -ForegroundColor White
    Write-Host "5. Follow WORKFLOW.md (MAIN/SIDE restrictions)" -ForegroundColor White
    
} finally {
    Pop-Location
}
