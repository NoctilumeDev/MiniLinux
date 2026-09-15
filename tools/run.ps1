param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$IsoPath = '',
    [int]$TimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$qemu = Join-Path $ToolRoot 'qemu-20260811\qemu-system-x86_64.exe'
$iso = if ($IsoPath) { $IsoPath } else {
    Join-Path $ToolRoot 'images\minilinux-m0.iso'
}
$log = Join-Path $project 'build\serial.log'
if (Test-Path -LiteralPath $log) { Clear-Content -LiteralPath $log }

$args = @('-machine', 'pc', '-m', '256M', '-smp', '1', '-accel', 'tcg',
          '-display', 'none', '-serial', 'file:build/serial.log',
          '-monitor', 'none', '-no-reboot', '-cdrom', $iso, '-boot', 'd')
$guest = Start-Process -FilePath $qemu -ArgumentList $args `
    -WorkingDirectory $project -WindowStyle Hidden -PassThru
try {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 250
        $output = if (Test-Path -LiteralPath $log) {
            [string](Get-Content -LiteralPath $log -Raw)
        } else { '' }
        if ($null -eq $output) { $output = '' }
        if ($output.Contains('M0 reached its intentional stop')) { break }
        if ($guest.HasExited) { break }
    } while ((Get-Date) -lt $deadline)
} finally {
    if (-not $guest.HasExited) { Stop-Process -Id $guest.Id }
}

$output = [string](Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue)
if ($null -eq $output) { $output = '' }
Write-Host $output
if (-not $output.Contains('MiniLinux M0: entered kernel_main') -or
    -not $output.Contains('MiniLinux PANIC: M0 reached its intentional stop')) {
    throw 'M0 serial evidence is missing. Check the image and bootloader.'
}
Write-Host 'M0 boot and serial evidence passed.'
