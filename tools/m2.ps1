param([string]$ToolRoot = 'D:\DevTools\MiniLinux')
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'verify-tools.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone M2
& (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone M2
& (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath (Join-Path $ToolRoot 'images\minilinux-m2.iso')
foreach ($memory in @(64, 256)) {
    & (Join-Path $PSScriptRoot 'run.ps1') -ToolRoot $ToolRoot -Milestone M2 -MemoryMiB $memory
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\build\serial.log') -Destination (Join-Path $PSScriptRoot "..\build\m2-serial-${memory}m.log")
}
