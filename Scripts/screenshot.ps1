# Starts the game, runs console commands, saves a screenshot with UI and quits.
# Usage: powershell -File Scripts/screenshot.ps1 [-Commands "DebugOpenNote elias_01"] [-Out shot.png]
param([string]$Commands = "", [string]$Out = "Saved\Screenshots\latest.png", [int]$Delay = 4)
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Project = Join-Path $PSScriptRoot "..\TheLastWatch.uproject" | Resolve-Path
$ShotDir = Join-Path $PSScriptRoot "..\Saved\Screenshots\WindowsEditor"
if (Test-Path $ShotDir) { Remove-Item "$ShotDir\*.png" -Force }

# "Shot showui" captures the frame including the HUD
$Exec = if ($Commands) { "$Commands, Shot showui" } else { "Shot showui" }
$p = Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -ArgumentList @(
    "`"$Project`"", "-game", "-windowed", "-ResX=1280", "-ResY=720", "-nosplash", "-log", "-ExecCmds=`"$Exec`"")

for ($i = 0; $i -lt 120; $i++) {
    Start-Sleep 2
    $png = Get-ChildItem $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($png) { break }
}
Start-Sleep $Delay
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

if ($png) { Copy-Item $png.FullName $Out -Force; "Saved $Out" } else { "No screenshot"; exit 1 }
