param(
    [ValidateSet('M2', 'M3')][string]$Milestone = 'M3',
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
Write-Host "$Milestone mechanism checks passed."
