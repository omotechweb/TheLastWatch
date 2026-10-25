# Runs an editor Python script. The editor must be closed.
# Usage: powershell -File Scripts/run_python.ps1 Scripts/Editor/<script>.py [-Editor]
#   default : headless commandlet (fast, no window)
#   -Editor : full editor with UI, quits when the script ends. Needed for anything that
#             touches Slate, e.g. FBX/texture import.
param([Parameter(Mandatory = $true)][string]$Script, [switch]$Editor)
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Project = Join-Path $PSScriptRoot "..\TheLastWatch.uproject" | Resolve-Path
$ScriptPath = Resolve-Path $Script

if ($Editor) {
    $env:TLW_SCRIPT = $ScriptPath
    $Bootstrap = Join-Path $PSScriptRoot "Editor\_run_and_quit.py"
    $Log = Join-Path $PSScriptRoot "..\Saved\Logs\TheLastWatch.log"
    $p = Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -Wait -ArgumentList @(
        "`"$Project`"", "-nosplash", "-unattended", "-ExecutePythonScript=`"$Bootstrap`"")
    Get-Content $Log
    exit $p.ExitCode
}

& "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project" -run=pythonscript "-script=$ScriptPath" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
exit $LASTEXITCODE
