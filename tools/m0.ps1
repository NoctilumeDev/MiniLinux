param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux'
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'verify-tools.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'run.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'debug.ps1') -ToolRoot $ToolRoot
