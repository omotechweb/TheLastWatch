# Automated input test: starts the game, focuses its window, sends real key presses and
# returns the log lines from the keeper's movement trace (tlw.LogPawn).
# Usage: powershell -File Scripts/test_input.ps1 -Commands "DebugTeleport ..." -Keys "WAIT:6;SPACE;WAIT:1.5"
#   Keys: WAIT:<seconds>, SPACE, CTRLDOWN, CTRLUP, or any SendKeys string (e.g. "e", "{TAB}").
#   CTRLDOWN/CTRLUP hold and release Left Ctrl with keybd_event (SendKeys cannot hold keys).
param([string]$Commands = "", [string]$Keys = "WAIT:8;SPACE;WAIT:2", [string]$Pattern = "Pawn x=|Jump pressed|Crouch")
$Engine  = "C:\Program Files\Epic Games\UE_5.7"
$Project = Join-Path $PSScriptRoot "..\TheLastWatch.uproject" | Resolve-Path
$Log = Join-Path $PSScriptRoot "..\Saved\Logs\TheLastWatch.log"

Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class KeyInput {
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
    public static void Down(byte vk) { keybd_event(vk, 0, 0, UIntPtr.Zero); }
    public static void Up(byte vk) { keybd_event(vk, 0, 2, UIntPtr.Zero); }
    // window messages reach the game even when Windows refuses to bring it to the front
    public static void PostDown(IntPtr hWnd, uint vk) {
        uint scan = MapVirtualKey(vk, 0);
        PostMessage(hWnd, 0x0100, (IntPtr)vk, (IntPtr)(1 | (scan << 16)));
    }
    public static void PostUp(IntPtr hWnd, uint vk) {
        uint scan = MapVirtualKey(vk, 0);
        PostMessage(hWnd, 0x0101, (IntPtr)vk, (IntPtr)(unchecked((int)(1 | (scan << 16) | (1u << 30) | (1u << 31)))));
    }
}
"@

$Exec = "tlw.LogPawn 1, DisableAllScreenMessages"
if ($Commands) { $Exec = "$Commands, $Exec" }
$p = Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -ArgumentList @(
    "`"$Project`"", "-game", "-windowed", "-ResX=1280", "-ResY=720", "-nosplash", "-log", "-ExecCmds=`"$Exec`"")

# start load times vary a lot (shader compiles, DDC); wait until the keeper is actually ticking
$ready = $false
for ($i = 0; $i -lt 600 -and -not $ready; $i++) {
    Start-Sleep -Milliseconds 500
    $ready = (Test-Path $Log) -and (Select-String -Path $Log -Pattern "Pawn x=" -Quiet)
}
if (-not $ready) { "game never became ready"; Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; exit 1 }
Start-Sleep 4  # let the keeper land

foreach ($key in $Keys.Split(";")) {
    if ($key -like "WAIT:*") { Start-Sleep -Milliseconds ([double]::Parse($key.Substring(5), [Globalization.CultureInfo]::InvariantCulture) * 1000); continue }
    $p.Refresh()
    [KeyInput]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 150
    $h = $p.MainWindowHandle
    # VK codes: SPACE 0x20, CTRL 0x11, TAB 0x09, E 0x45, W 0x57, C 0x43
    $vk = @{ "SPACE" = 0x20; "TAB" = 0x09; "E" = 0x45; "C" = 0x43 }
    if ($key -match "^(\w+)DOWN$" -and $vk.ContainsKey($Matches[1]) -or $key -eq "CTRLDOWN" -or $key -eq "WDOWN") {
        $code = if ($key -eq "CTRLDOWN") { 0x11 } elseif ($key -eq "WDOWN") { 0x57 } else { $vk[$Matches[1]] }
        [KeyInput]::PostDown($h, $code)
    } elseif ($key -eq "CTRLUP" -or $key -eq "WUP") {
        [KeyInput]::PostUp($h, $(if ($key -eq "CTRLUP") { 0x11 } else { 0x57 }))
    } elseif ($key -match "^(\w+)UP$" -and $vk.ContainsKey($Matches[1])) {
        [KeyInput]::PostUp($h, $vk[$Matches[1]])
    } elseif ($vk.ContainsKey($key)) {
        [KeyInput]::PostDown($h, $vk[$key]); Start-Sleep -Milliseconds 150; [KeyInput]::PostUp($h, $vk[$key])
    } else {
        [System.Windows.Forms.SendKeys]::SendWait($key)
    }
    "sent $key at $(Get-Date -Format HH:mm:ss.fff)"
}
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Start-Sleep 1
Select-String -Path $Log -Pattern $Pattern | ForEach-Object { $_.Line }
