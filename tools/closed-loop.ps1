param([string]$ToolRoot = 'D:\DevTools\MiniLinux')
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $project 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null

# Run one guest at a time. Each stage rebuilds its own ELF and verifies its ISO.
foreach ($level in 0..6) {
    $stage = "M$level"
    $log = Join-Path $build "$stage-round.log"
    Write-Host "Checking $stage ..."
    try {
        if ($level -eq 0) { & (Join-Path $PSScriptRoot 'm0.ps1') -ToolRoot $ToolRoot *> $log }
        elseif ($level -eq 1) { & (Join-Path $PSScriptRoot 'm1.ps1') -ToolRoot $ToolRoot *> $log }
        else { & (Join-Path $PSScriptRoot 'stage.ps1') -ToolRoot $ToolRoot -Milestone $stage *> $log }
        Copy-Item -LiteralPath (Join-Path $build 'gdb-trace.log') -Destination (Join-Path $build "$stage-gdb.log")
        if ($level -eq 3) {
            & (Join-Path $PSScriptRoot 'debug.ps1') -ToolRoot $ToolRoot -Milestone M3 -CorruptRegister *> (Join-Path $build 'M3-register-rejection.log')
        }
        if ($level -eq 6) {
            & (Join-Path $PSScriptRoot 'debug.ps1') -ToolRoot $ToolRoot -Milestone M6 -AliasUserData *> (Join-Path $build 'M6-alias-rejection.log')
            & (Join-Path $PSScriptRoot 'run.ps1') -ToolRoot $ToolRoot -Milestone M6 *> (Join-Path $build 'M6-normal-retake.log')
        }
    } catch {
        Get-Content -LiteralPath $log -Tail 15
        throw
    }
    Write-Host "$stage passed."
}

$llvm = Join-Path $ToolRoot 'llvm-23.1.1\clang+llvm-23.1.1-x86_64-pc-windows-msvc\bin'
foreach ($name in @('kernel.elf', 'user.elf')) {
    $elf = Join-Path $build $name
    $undefined = & (Join-Path $llvm 'llvm-nm.exe') --undefined-only $elf
    if ($LASTEXITCODE -ne 0 -or $undefined) { throw "Unresolved runtime symbol in $name" }
    $segments = & (Join-Path $llvm 'llvm-readelf.exe') -l $elf
    if ($LASTEXITCODE -ne 0 -or ($segments -match '\b(INTERP|DYNAMIC)\b')) { throw "Dynamic runtime segment in $name" }
}
$guests = @(Get-Process -Name qemu-system-x86_64,gdb -ErrorAction SilentlyContinue)
$listeners = @(Get-NetTCPConnection -LocalPort 1234 -State Listen -ErrorAction SilentlyContinue)
if ($guests.Count -ne 0 -or $listeners.Count -ne 0) { throw 'Guest/debugger remains after the experiment.' }
Write-Host 'M0-M6 closed loop passed; no unresolved or dynamic runtime dependencies; no guest/debugger residue.'
