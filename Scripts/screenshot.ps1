# Starts the game, runs console commands, saves a screenshot with UI after -Wait seconds and quits.
# Usage: powershell -File Scripts/screenshot.ps1 [-Commands "DebugOpenNote elias_01"] [-Out shot.png] [-Wait 8]
# Commands are comma separated; DebugTeleport X Y Z Yaw Pitch moves the camera.
param([string]$Commands = "", [string]$Out = "Saved\Screenshots\latest.png", [double]$Wait = 8)
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Project = Join-Path $PSScriptRoot "..\TheLastWatch.uproject" | Resolve-Path
$ShotDir = Join-Path $PSScriptRoot "..\Saved\Screenshots\WindowsEditor"
if (Test-Path $ShotDir) { Remove-Item "$ShotDir\*.png" -Force }

# DebugShotAfter waits for streaming/shader compilation before capturing frame + HUD
$Exec = "DisableAllScreenMessages, DebugShotAfter $Wait"
if ($Commands) { $Exec = "$Commands, $Exec" }
$p = Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -ArgumentList @(
    "`"$Project`"", "-game", "-windowed", "-ResX=1280", "-ResY=720", "-nosplash", "-log", "-ExecCmds=`"$Exec`"")

$png = $null
for ($i = 0; $i -lt 150; $i++) {
    Start-Sleep 2
    $png = Get-ChildItem $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($png) { break }
}
Start-Sleep 2
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

if ($png) { Copy-Item $png.FullName $Out -Force; "Saved $Out" } else { "No screenshot"; exit 1 }
