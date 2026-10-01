param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$Gdb = 'D:\GCC\mingw64\bin\gdb.exe',
    [string]$IsoPath = '',
    [int]$TimeoutSeconds = 20,
    [ValidateSet('M0', 'M1')][string]$Milestone = 'M0'
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$qemu = Join-Path $ToolRoot 'qemu-20260811\qemu-system-x86_64.exe'
$iso = if ($IsoPath) { $IsoPath } else {
    Join-Path $ToolRoot ('images\minilinux-' + $Milestone.ToLowerInvariant() + '.iso')
}
$kernel = (Resolve-Path -LiteralPath (Join-Path $project 'build\kernel.elf')).Path.Replace('\', '/')
$log = Join-Path $project 'build\gdb-serial.log'
if (Test-Path -LiteralPath $log) { Clear-Content -LiteralPath $log }
$commandsPath = Join-Path $project 'build\m0-debug.gdb'
$stdoutPath = Join-Path $project 'build\gdb-stdout.log'
$stderrPath = Join-Path $project 'build\gdb-stderr.log'
$tracePath = Join-Path $project 'build\gdb-trace.log'
$commands = @(
    "file `"$kernel`"",
    'target remote 127.0.0.1:1234',
    'break kernel_main', 'continue', 'info registers rip cs'
)
$panicBreakpoint = 2
if ($Milestone -eq 'M1') {
    $commands += @('break page_alloc', 'continue', 'p pages.frame_count',
                   'p pages.free_count', 'p/x pages.metadata_base', 'disable 2')
    $panicBreakpoint = 3
}
$commands += @('break panic', 'continue', 'bt', 'detach')
Set-Content -LiteralPath $commandsPath -Value $commands
if (Get-NetTCPConnection -LocalPort 1234 -State Listen -ErrorAction SilentlyContinue) {
    throw 'GDB port 1234 is already in use.'
}

$args = @('-machine', 'pc', '-m', '256M', '-smp', '1', '-accel', 'tcg',
          '-display', 'none', '-serial', 'file:build/gdb-serial.log',
          '-monitor', 'none', '-no-reboot', '-cdrom', $iso, '-boot', 'd',
          '-S', '-gdb', 'tcp:127.0.0.1:1234')
$guest = Start-Process -FilePath $qemu -ArgumentList $args `
    -WorkingDirectory $project -WindowStyle Hidden -PassThru
$debugger = $null
try {
    Start-Sleep -Seconds 2
    $scriptArg = '"' + $commandsPath.Replace('\', '/') + '"'
    $debugger = Start-Process -FilePath $Gdb `
        -ArgumentList @('-batch', '-x', $scriptArg) `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath `
        -WindowStyle Hidden -PassThru
    $timedOut = $false
    if (-not $debugger.HasExited) {
        try {
            Wait-Process -Id $debugger.Id -Timeout $TimeoutSeconds -ErrorAction Stop
        } catch {
            if (-not $debugger.HasExited) { $timedOut = $true }
        }
    }
    if ($timedOut) { Stop-Process -Id $debugger.Id }
    $trace = [string](Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue)
    $errors = [string](Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue)
    $trace += $errors
    Set-Content -LiteralPath $tracePath -Value $trace
    Write-Host $trace
    if ($timedOut) { throw "GDB exceeded the $TimeoutSeconds-second observation limit." }
    if ($debugger.ExitCode -ne 0 -or
        -not $trace.Contains('Breakpoint 1, kernel_main') -or
        -not $trace.Contains("Breakpoint $panicBreakpoint, panic") -or
        ($Milestone -eq 'M1' -and -not $trace.Contains('Breakpoint 2, page_alloc'))) {
        throw "GDB did not hit the $Milestone breakpoints."
    }
    Start-Sleep -Milliseconds 500
} finally {
    if ($debugger -and -not $debugger.HasExited) { Stop-Process -Id $debugger.Id }
    if (-not $guest.HasExited) { Stop-Process -Id $guest.Id }
}
Get-Content -LiteralPath $log
if ($Milestone -eq 'M1') {
    $serial = [string](Get-Content -LiteralPath $log -Raw)
    if (-not $serial.Contains('MiniLinux M1: physical page checks passed') -or
        -not $serial.Contains('MiniLinux PANIC: M1 reached its intentional stop')) {
        throw 'GDB did not observe completed M1 page checks and the intentional stop.'
    }
}
