param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$Python = 'D:\python-3.10.6\python.exe',
    [ValidateSet('M0', 'M1', 'M2')][string]$Milestone = 'M0'
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$env:PYTHONPATH = Join-Path $ToolRoot 'python-packages'
& $Python (Join-Path $PSScriptRoot 'make-image.py') $project $ToolRoot $Milestone
if ($LASTEXITCODE -ne 0) { throw 'ISO creation failed.' }

$iso = Join-Path $ToolRoot ('images\minilinux-' + $Milestone.ToLowerInvariant() + '.iso')
Write-Host "Created $iso"
