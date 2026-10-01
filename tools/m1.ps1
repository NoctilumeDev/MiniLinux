param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux'
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'verify-tools.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone M1
& (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone M1
$iso = Join-Path $ToolRoot 'images\minilinux-m1.iso'
& (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath $iso
foreach ($memory in @(64, 256)) {
    & (Join-Path $PSScriptRoot 'run.ps1') -ToolRoot $ToolRoot -Milestone M1 -MemoryMiB $memory
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\build\serial.log') `
        -Destination (Join-Path $PSScriptRoot "..\build\m1-serial-${memory}m.log")
}
& (Join-Path $PSScriptRoot 'debug.ps1') -ToolRoot $ToolRoot -Milestone M1
Write-Host 'M1 physical page experiment passed at 64 MiB and 256 MiB.'
