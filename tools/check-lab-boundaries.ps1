param([string]$ToolRoot = 'D:\DevTools\MiniLinux')
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$fixture = Join-Path $project 'build\lab-boundaries'
$runtime = Join-Path $fixture 'runtime'
$output = Join-Path $project 'build\supplemental'
$patch = Join-Path $project 'docs\evidence\experience-regression\fixture.patch'
$python = 'D:\python-3.10.6\python.exe'

if (Get-Process -Name qemu-system-x86_64,gdb -ErrorAction SilentlyContinue) {
    throw 'Stop existing guests/debuggers before running these sequential probes.'
}
if (Test-Path -LiteralPath $fixture) { throw 'Existing boundary fixture needs inspection before reuse.' }
if ((Get-Item -LiteralPath (Join-Path $project 'build') -ErrorAction SilentlyContinue).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw 'Build directory must not be a junction or symlink.'
}

# Build today's normal LAB first; the historical patch supplies only test input.
& (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone LAB
& (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone LAB
& (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath (Join-Path $ToolRoot 'images\minilinux-lab.iso')
& $python (Join-Path $PSScriptRoot 'test-lab-boundaries.py') --mode baseline --project $project --tool-root $ToolRoot --output $output
if ($LASTEXITCODE -ne 0) { throw 'Normal LAB boundary probes failed; see build/supplemental.' }

New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    foreach ($name in @('boot', 'include', 'kernel', 'user')) {
        Copy-Item -LiteralPath (Join-Path $project $name) -Destination $fixture -Recurse
    }
    Copy-Item -LiteralPath (Join-Path $project 'linker.ld') -Destination $fixture
    New-Item -ItemType Directory -Path (Join-Path $fixture 'tools'), $runtime | Out-Null
    foreach ($name in @('build.ps1', 'image.ps1', 'make-image.py', 'verify-image.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $fixture 'tools')
    }
    # Private images, shared fixed tools. Never copy or delete the borrowed tools.
    foreach ($name in @('llvm-23.1.1', 'limine-12.9.0', 'qemu-20260811', 'python-packages')) {
        New-Item -ItemType Junction -Path (Join-Path $runtime $name) -Target (Join-Path $ToolRoot $name) | Out-Null
    }
    Push-Location $project
    try {
        & git apply --no-index --exclude=tools/* --directory=build/lab-boundaries $patch
        if ($LASTEXITCODE -ne 0) { throw 'Retained user probe patch no longer applies; update the witness explicitly.' }
    }
    finally { Pop-Location }
    & (Join-Path $fixture 'tools\build.ps1') -ToolRoot $runtime -Milestone LAB
    & (Join-Path $fixture 'tools\image.ps1') -ToolRoot $runtime -Milestone LAB
    & (Join-Path $fixture 'tools\verify-image.ps1') -ToolRoot $runtime -IsoPath (Join-Path $runtime 'images\minilinux-lab.iso')
    & $python (Join-Path $PSScriptRoot 'test-lab-boundaries.py') --mode fixture --project $fixture --tool-root $runtime --output $output
    if ($LASTEXITCODE -ne 0) { throw 'User fixture probes failed; see build/supplemental.' }
}
finally {
    $build = [IO.Path]::GetFullPath((Join-Path $project 'build')) + [IO.Path]::DirectorySeparatorChar
    $resolved = (Resolve-Path -LiteralPath $fixture).Path
    if (-not $resolved.StartsWith($build, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Fixture cleanup escaped the build directory.'
    }
    foreach ($name in @('llvm-23.1.1', 'limine-12.9.0', 'qemu-20260811', 'python-packages')) {
        $link = Join-Path $runtime $name
        if (Test-Path -LiteralPath $link) { [IO.Directory]::Delete($link) }
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
Write-Host 'LAB supplemental boundaries passed; private fixture and tool links removed.'
