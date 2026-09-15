param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$Gdb = 'D:\GCC\mingw64\bin\gdb.exe',
    [string]$IsoPath = '',
    [int]$TimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$qemu = Join-Path $ToolRoot 'qemu-20260811\qemu-system-x86_64.exe'
$iso = if ($IsoPath) { $IsoPath } else {
    Join-Path $ToolRoot 'images\minilinux-m0.iso'
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
    'break kernel_main', 'continue', 'info registers rip cs',
    'break panic', 'continue', 'bt', 'detach'
)
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
        -not $trace.Contains('Breakpoint 2, panic')) {
        throw 'GDB did not hit both M0 breakpoints.'
    }
    Start-Sleep -Milliseconds 500
} finally {
    if ($debugger -and -not $debugger.HasExited) { Stop-Process -Id $debugger.Id }
    if (-not $guest.HasExited) { Stop-Process -Id $guest.Id }
}
Get-Content -LiteralPath $log
