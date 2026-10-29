# Packages a Shipping build of the game and wraps it in an Inno Setup installer.
# Usage: powershell -File Scripts/build_installer.ps1 [-Version 0.1.0] [-SkipPackage]
# Result: Saved\Installer\TheLastWatch-Setup-<version>.exe  (packaged game in Saved\Package\Windows)
# The editor must be closed. All Fab packs listed in CREDITS.md must be in Content/.
param([string]$Version = "0.1.0", [switch]$SkipPackage)
$ErrorActionPreference = "Stop"
$Root    = Resolve-Path (Join-Path $PSScriptRoot "..")
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Package = Join-Path $Root "Saved\Package"

if (-not $SkipPackage) {
    New-Item -ItemType Directory -Force $Package | Out-Null
    & "$Engine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
        -project="$Root\TheLastWatch.uproject" -platform=Win64 -clientconfig=Shipping `
        -build -cook -stage -pak -iostore -compressed -prereqs -package `
        -archive -archivedirectory="$Package" -map=/Game/Maps/L_Skerrow `
        -nop4 -utf8output -unattended
    if ($LASTEXITCODE -ne 0) { throw "Packaging failed ($LASTEXITCODE)" }
}

$Game = Join-Path $Package "Windows"
if (-not (Test-Path "$Game\TheLastWatch.exe")) { throw "No packaged game in $Game" }

$Iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
          "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
          "$env:ProgramFiles\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Iscc) { throw "Inno Setup 6 not found (winget install JRSoftware.InnoSetup)" }

& $Iscc /Q "/DGameDir=$Game" "/DAppVersion=$Version" (Join-Path $PSScriptRoot "Installer\TheLastWatch.iss")
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed ($LASTEXITCODE)" }
Get-Item (Join-Path $Root "Saved\Installer\TheLastWatch-Setup-$Version.exe") | Select-Object FullName, @{n="MB";e={[math]::Round($_.Length/1MB)}}
