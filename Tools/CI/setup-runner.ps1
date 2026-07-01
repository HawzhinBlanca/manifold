<#
.SYNOPSIS
    Register this Windows machine as a self-hosted GitHub Actions runner for MANIFOLD,
    with the labels the CI workflow expects: self-hosted, windows, unreal.

.DESCRIPTION
    Unreal Engine is not available on GitHub-hosted runners, so MANIFOLD CI
    (.github/workflows/ci.yml) runs on a self-hosted runner that has UE 5.8, the
    VS2022 C++ toolchain, the .NET Framework 4.8 SDK, and Git LFS installed.

    This script downloads the runner agent, configures it against
    HawzhinBlanca/manifold with the correct labels, and (optionally) installs it as
    a Windows service so CI runs automatically.

.PARAMETER RegistrationToken
    A runner registration token. Get one from:
      GitHub repo -> Settings -> Actions -> Runners -> New self-hosted runner
    (the token shown in the `./config.cmd --token XXXX` line). Tokens expire ~1h.

.PARAMETER InstallService
    Install + start the runner as a Windows service (recommended for always-on CI).

.EXAMPLE
    .\Tools\CI\setup-runner.ps1 -RegistrationToken A1B2C3... -InstallService
#>
param(
    [Parameter(Mandatory = $true)] [string] $RegistrationToken,
    [switch] $InstallService,
    [string] $RepoUrl = "https://github.com/HawzhinBlanca/manifold",
    [string] $RunnerVersion = "2.322.0",
    [string] $InstallDir = "C:\actions-runner-manifold"
)

$ErrorActionPreference = "Stop"

Write-Host "== MANIFOLD self-hosted runner setup ==" -ForegroundColor Cyan

if (-not (Test-Path $InstallDir)) { New-Item -ItemType Directory -Path $InstallDir | Out-Null }
Set-Location $InstallDir

$zip = "actions-runner-win-x64-$RunnerVersion.zip"
$url = "https://github.com/actions/runner/releases/download/v$RunnerVersion/$zip"
if (-not (Test-Path (Join-Path $InstallDir "config.cmd"))) {
    Write-Host "Downloading runner $RunnerVersion ..." -ForegroundColor Gray
    Invoke-WebRequest -Uri $url -OutFile (Join-Path $InstallDir $zip)
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $InstallDir $zip), $InstallDir)
}

$labels = "self-hosted,windows,unreal"
$configArgs = @(
    "--url", $RepoUrl,
    "--token", $RegistrationToken,
    "--labels", $labels,
    "--name", "$env:COMPUTERNAME-manifold",
    "--unattended",
    "--replace"
)

Write-Host "Configuring runner (labels: $labels) ..." -ForegroundColor Gray
& (Join-Path $InstallDir "config.cmd") @configArgs

if ($InstallService) {
    Write-Host "Installing + starting runner service (run this shell as Administrator) ..." -ForegroundColor Gray
    & (Join-Path $InstallDir "svc.cmd") install
    & (Join-Path $InstallDir "svc.cmd") start
    Write-Host "Runner service installed and started." -ForegroundColor Green
} else {
    Write-Host "Configured. Start it now with:  .\run.cmd" -ForegroundColor Green
    Write-Host "(or re-run with -InstallService to run as a Windows service)" -ForegroundColor Green
}

Write-Host "`nOnce online, add the required status check to branch protection:" -ForegroundColor Cyan
Write-Host "  Settings -> Branches -> main -> Require status checks -> 'build-and-test'" -ForegroundColor Cyan
