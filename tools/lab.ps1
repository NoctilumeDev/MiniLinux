param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [ValidateRange(1024, 65535)][int]$Port = 8080,
    [ValidateSet(64, 256)][int]$MemoryMiB = 64
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'verify-tools.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone LAB
& (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone LAB
& (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath (Join-Path $ToolRoot 'images\minilinux-lab.iso')
& 'D:\python-3.10.6\python.exe' (Join-Path $PSScriptRoot 'lab-server.py') --port $Port --memory $MemoryMiB --tool-root $ToolRoot
if ($LASTEXITCODE -ne 0) { throw 'Laboratory bridge stopped with an error.' }
