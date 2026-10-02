param(
    [ValidateSet('M2', 'M3', 'M4', 'M5', 'M6')][string]$Milestone = 'M6',
    [string]$ToolRoot = 'D:\DevTools\MiniLinux'
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'verify-tools.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone $Milestone
& (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone $Milestone
$iso = Join-Path $ToolRoot ('images\minilinux-' + $Milestone.ToLowerInvariant() + '.iso')
& (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath $iso
foreach ($memory in @(64, 256)) {
    & (Join-Path $PSScriptRoot 'run.ps1') -ToolRoot $ToolRoot -Milestone $Milestone -MemoryMiB $memory
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\build\serial.log') -Destination (Join-Path $PSScriptRoot "..\build\$Milestone-serial-${memory}m.log")
}
& (Join-Path $PSScriptRoot 'debug.ps1') -ToolRoot $ToolRoot -Milestone $Milestone
if ($Milestone -eq 'M6') {
    foreach ($probe in @(2, 3, 4, 5, 6, 7)) {
        & (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone M6 -FaultCase $probe
        & (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone M6
        & (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath $iso
        & (Join-Path $PSScriptRoot 'run.ps1') -ToolRoot $ToolRoot -Milestone M6
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\build\serial.log') -Destination (Join-Path $PSScriptRoot "..\build\M6-probe-$probe.log")
    }
    & (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone M6
    & (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone M6
    & (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath $iso
}
Write-Host "$Milestone mechanism checks passed."
