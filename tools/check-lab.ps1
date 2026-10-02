param([string]$ToolRoot = 'D:\DevTools\MiniLinux')
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'verify-tools.ps1') -ToolRoot $ToolRoot
& (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone LAB
& (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone LAB
& (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath (Join-Path $ToolRoot 'images\minilinux-lab.iso')
$llvm = Join-Path $ToolRoot 'llvm-23.1.1\clang+llvm-23.1.1-x86_64-pc-windows-msvc\bin'
foreach ($name in @('kernel.elf', 'user.elf')) {
    $elf = Join-Path $PSScriptRoot "..\build\$name"
    $undefined = & (Join-Path $llvm 'llvm-nm.exe') --undefined-only $elf
    if ($LASTEXITCODE -ne 0 -or $undefined) { throw "Unresolved runtime symbol in $name" }
    $segments = & (Join-Path $llvm 'llvm-readelf.exe') -l $elf
    if ($LASTEXITCODE -ne 0 -or ($segments -match '\b(INTERP|DYNAMIC)\b')) { throw "Dynamic runtime segment in $name" }
}
foreach ($test in @('test-userland.py', 'test-lab-bridge.py')) {
    & 'D:\python-3.10.6\python.exe' (Join-Path $PSScriptRoot $test) --tool-root $ToolRoot
    if ($LASTEXITCODE -ne 0) { throw "LAB check failed: $test" }
}
Write-Host 'LAB closed loop passed; no unresolved or dynamic runtime dependencies.'
