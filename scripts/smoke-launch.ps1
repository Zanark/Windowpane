$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$executable = Join-Path $root 'build\Release\WindowPane.exe'
if (-not (Test-Path $executable)) { throw 'Build WindowPane first.' }
if (Get-Process -Name WindowPane -ErrorAction SilentlyContinue) {
    throw 'WindowPane is already running. Close it before this bounded startup test.'
}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WindowPaneSmoke {
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessageW(IntPtr hwnd, uint message, UIntPtr wparam, IntPtr lparam);
}
'@
$process = Start-Process $executable -PassThru
try {
    $deadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        $window = $process.MainWindowHandle
    } while ($window -eq [IntPtr]::Zero -and -not $process.HasExited -and (Get-Date) -lt $deadline)
    if ($window -eq [IntPtr]::Zero -or $process.HasExited) { throw 'Normal controls did not become ready.' }
    if (-not [WindowPaneSmoke]::IsWindowVisible($window)) { throw 'Controls are not visible.' }
    if (-not [WindowPaneSmoke]::PostMessageW($window, 0x0010, [UIntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'Cannot exercise close-to-tray.'
    }
    Start-Sleep -Milliseconds 300
    if ([WindowPaneSmoke]::IsWindowVisible($window) -or $process.HasExited) {
        throw 'Close did not preserve the tray-backed process.'
    }
    if (-not [WindowPaneSmoke]::PostMessageW($window, 0x8002, [UIntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'Cannot request controls recovery.'
    }
    Start-Sleep -Milliseconds 300
    if (-not [WindowPaneSmoke]::IsWindowVisible($window)) { throw 'Controls recovery failed.' }
    if (-not [WindowPaneSmoke]::PostMessageW($window, 0x0111, [UIntPtr]::new(111), [IntPtr]::Zero)) {
        throw 'Cannot request normal application exit.'
    }
    if (-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0) { throw 'Normal exit failed.' }
    Write-Output 'PASS normal startup, close-to-tray, controls recovery and clean exit'
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id }
    $process.Dispose()
}
