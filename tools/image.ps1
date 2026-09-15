param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$Python = 'D:\python-3.10.6\python.exe'
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$env:PYTHONPATH = Join-Path $ToolRoot 'python-packages'
& $Python (Join-Path $PSScriptRoot 'make-image.py') $project $ToolRoot
if ($LASTEXITCODE -ne 0) { throw 'ISO creation failed.' }

$iso = Join-Path $ToolRoot 'images\minilinux-m0.iso'
Write-Host "Created $iso"
