<#
.SYNOPSIS
    MANIFOLD packaging - produce a standalone, runnable Windows build.

.DESCRIPTION
    Cooks content, builds the game (client) target, stages, paks, and archives a
    shippable build to dist/. The packaged game boots into the default map (an engine
    map) whose GameMode is ManifoldGameMode, so it runs the full vertical-slice loop
    (simulation + HUD + debug-draw visualizer) with the [E]/[R] key verbs.

    Runs headless via UE's RunUAT + bundled toolchain.

.EXAMPLE
    # Development build (fast, with logging/console)
    .\Tools\CI\package.ps1

    # Shipping build (optimized, no console)
    .\Tools\CI\package.ps1 -Config Shipping
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Development","Shipping","Debug")]
    [string]$Config = "Development",

    [Parameter(Mandatory=$false)]
    [string]$UE5Path = "C:\Program Files\Epic Games\UE_5.8",

    [Parameter(Mandatory=$false)]
    [string]$ProjectPath = "",

    [Parameter(Mandatory=$false)]
    [string]$ArchiveDir = ""
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
if (-not $ProjectPath) { $ProjectPath = $repoRoot }

$uproject = Get-ChildItem -Path $ProjectPath -Filter "*.uproject" | Select-Object -First 1
if (-not $uproject) { Write-Error "No .uproject found under $ProjectPath"; exit 1 }
if (-not $ArchiveDir) { $ArchiveDir = Join-Path $repoRoot "dist" }

$runUAT = Join-Path $UE5Path "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path $runUAT)) { Write-Error "RunUAT not found at $runUAT. Set -UE5Path."; exit 1 }

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  MANIFOLD PACKAGING" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Project:  $($uproject.FullName)"
Write-Host "Config:   $Config"
Write-Host "Archive:  $ArchiveDir"
Write-Host ""

# Standard BuildCookRun for a client build, archived to dist/.
$uatArgs = @(
    "BuildCookRun",
    "-project=`"$($uproject.FullName)`"",
    "-noP4",
    "-platform=Win64",
    "-clientconfig=$Config",
    "-cook",
    "-build",
    "-stage",
    "-pak",
    "-archive",
    "-archivedirectory=`"$ArchiveDir`"",
    "-unattended",
    "-nocompileeditor",
    "-utf8output"
)

Write-Host "RunUAT $($uatArgs -join ' ')" -ForegroundColor Gray
cmd /c "`"$runUAT`" $($uatArgs -join ' ')"
$exitCode = $LASTEXITCODE

if ($exitCode -eq 0) {
    Write-Host "`n=== PACKAGE: SUCCESS ===" -ForegroundColor Green
    Write-Host "Build archived under: $ArchiveDir\Windows" -ForegroundColor Green
} else {
    Write-Host "`n=== PACKAGE: FAIL (exit $exitCode) ===" -ForegroundColor Red
}
exit $exitCode
