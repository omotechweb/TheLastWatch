# Runs an editor Python script headless (no editor window). The editor must be closed.
# Usage: powershell -File Scripts/run_python.ps1 Scripts/Editor/<script>.py
param([Parameter(Mandatory = $true)][string]$Script)
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Project = Join-Path $PSScriptRoot "..\TheLastWatch.uproject" | Resolve-Path
$ScriptPath = Resolve-Path $Script

& "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project" -run=pythonscript "-script=$ScriptPath" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
exit $LASTEXITCODE
