param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$Gdb = 'D:\GCC\mingw64\bin\gdb.exe'
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$qemu = Join-Path $ToolRoot 'qemu-20260811\qemu-system-x86_64.exe'
$iso = Join-Path $ToolRoot 'images\minilinux-m0.iso'
$kernel = (Resolve-Path -LiteralPath (Join-Path $project 'build\kernel.elf')).Path.Replace('\', '/')
$log = Join-Path $project 'build\gdb-serial.log'
if (Test-Path -LiteralPath $log) { Clear-Content -LiteralPath $log }

$args = @('-machine', 'pc', '-m', '256M', '-smp', '1', '-accel', 'tcg',
          '-display', 'none', '-serial', 'file:build/gdb-serial.log',
          '-monitor', 'none', '-no-reboot', '-cdrom', $iso, '-boot', 'd',
          '-S', '-gdb', 'tcp:127.0.0.1:1234')
$guest = Start-Process -FilePath $qemu -ArgumentList $args `
    -WorkingDirectory $project -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds 2
    $trace = & $Gdb -batch -ex "file $kernel" `
        -ex 'target remote 127.0.0.1:1234' `
        -ex 'break kernel_main' -ex 'continue' `
        -ex 'info registers rip cs' `
        -ex 'break panic' -ex 'continue' -ex 'bt' -ex 'detach' 2>&1 | Out-String
    $status = $LASTEXITCODE
    Set-Content -LiteralPath (Join-Path $project 'build\gdb-trace.log') -Value $trace
    Write-Host $trace
    if ($status -ne 0 -or
        -not $trace.Contains('Breakpoint 1, kernel_main') -or
        -not $trace.Contains('Breakpoint 2, panic')) {
        throw 'GDB did not hit both M0 breakpoints.'
    }
    Start-Sleep -Milliseconds 500
} finally {
    if (-not $guest.HasExited) { Stop-Process -Id $guest.Id }
}
Get-Content -LiteralPath $log
