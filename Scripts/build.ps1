# Builds the editor target from the command line.
# The editor must be closed (or Live Coding disabled), otherwise the build is blocked.
$ErrorActionPreference = "Stop"
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Project = Join-Path $PSScriptRoot "..\TheLastWatch.uproject" | Resolve-Path

& "$Engine\Engine\Build\BatchFiles\Build.bat" TheLastWatchEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
exit $LASTEXITCODE
