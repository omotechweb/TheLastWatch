# Launches the game directly (no editor UI). Optional: -Map /Game/Path/To/Map
param([string]$Map = "")
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Project = Join-Path $PSScriptRoot "..\TheLastWatch.uproject" | Resolve-Path

$GameArgs = @("$Project")
if ($Map) { $GameArgs += $Map }
$GameArgs += @("-game", "-windowed", "-ResX=1600", "-ResY=900", "-log")

Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $GameArgs
