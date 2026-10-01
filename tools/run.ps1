param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$IsoPath = '',
    [int]$TimeoutSeconds = 20,
    [ValidateSet('M0', 'M1', 'M2')][string]$Milestone = 'M0',
    [ValidateRange(64, 256)][int]$MemoryMiB = 256
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$qemu = Join-Path $ToolRoot 'qemu-20260811\qemu-system-x86_64.exe'
$iso = if ($IsoPath) { $IsoPath } else {
    Join-Path $ToolRoot ('images\minilinux-' + $Milestone.ToLowerInvariant() + '.iso')
}
$log = Join-Path $project 'build\serial.log'
if (Test-Path -LiteralPath $log) { Clear-Content -LiteralPath $log }

$args = @('-machine', 'pc', '-m', ($MemoryMiB.ToString() + 'M'), '-smp', '1', '-accel', 'tcg',
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
        if ($output.Contains("$Milestone reached its intentional stop")) { break }
        if ($guest.HasExited) { break }
    } while ((Get-Date) -lt $deadline)
} finally {
    if (-not $guest.HasExited) { Stop-Process -Id $guest.Id }
}

$output = [string](Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue)
if ($null -eq $output) { $output = '' }
Write-Host $output
if (-not $output.Contains("MiniLinux ${Milestone}: entered kernel_main") -or
    -not $output.Contains("MiniLinux PANIC: $Milestone reached its intentional stop")) {
    throw "$Milestone serial evidence is missing. Check the image and bootloader."
}
if ($Milestone -eq 'M1' -and -not $output.Contains('MiniLinux M1: physical page checks passed')) {
    throw 'M1 physical page checks did not finish.'
}
if ($Milestone -eq 'M2' -and -not $output.Contains('MiniLinux M2: address mapping checks passed')) {
    throw 'M2 address mapping checks did not finish.'
}
Write-Host "$Milestone boot and serial evidence passed ($MemoryMiB MiB guest)."
