param([string]$ToolRoot = 'D:\DevTools\MiniLinux')
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$logs = Join-Path $project 'build\counterexamples'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$iso = Join-Path $ToolRoot 'images\minilinux-m6.iso'
$summary = Join-Path $logs 'summary.log'
$source = & git -C $project rev-parse HEAD
$dirty = & git -C $project status --porcelain
Set-Content -LiteralPath $summary -Value @("source=$source", "working-tree-dirty=$([bool]$dirty)")

function Build-Case([int]$attack, [int]$fault, [string]$name) {
    & (Join-Path $PSScriptRoot 'build.ps1') -ToolRoot $ToolRoot -Milestone M6 -AttackCase $attack -FaultCase $fault *> (Join-Path $logs "$name-build.log")
    & (Join-Path $PSScriptRoot 'image.ps1') -ToolRoot $ToolRoot -Milestone M6 *> (Join-Path $logs "$name-image.log")
    & (Join-Path $PSScriptRoot 'verify-image.ps1') -ToolRoot $ToolRoot -IsoPath $iso *> (Join-Path $logs "$name-payload.log")
    $hash = (Get-FileHash -LiteralPath (Join-Path $project 'build\kernel.elf') -Algorithm SHA256).Hash
    Add-Content -LiteralPath $summary -Value "$name attack=$attack fault=$fault elf=$hash payload=matched"
}

function Run-Case([string]$name, [int]$memory, [string]$marker) {
    & (Join-Path $PSScriptRoot 'run.ps1') -ToolRoot $ToolRoot -Milestone M6 -MemoryMiB $memory *> (Join-Path $logs "$name-${memory}m-observer.log")
    $serial = [string](Get-Content -LiteralPath (Join-Path $project 'build\serial.log') -Raw)
    Set-Content -LiteralPath (Join-Path $logs "$name-${memory}m-serial.log") -Value $serial
    if (-not $serial.Contains($marker)) { throw "Missing counterexample witness: $name" }
    Add-Content -LiteralPath $summary -Value "$name memory=$memory passed"
    Write-Host "$name passed ($memory MiB)."
}

try {
    & (Join-Path $PSScriptRoot 'verify-tools.ps1') -ToolRoot $ToolRoot *> (Join-Path $logs 'tools.log')
    $markers = @('', 'counterexample: syscall parent permission rejected',
                    'counterexample: all ancestor permissions checked; pages restored',
                    'counterexample: table limit rejected new branch; existing branch and cleanup passed',
                    'counterexample: pointer endpoints checked without consuming file bytes',
                    'counterexample: DF and nine registers survived timer and syscall entry')
    foreach ($attack in 1..5) {
        $name = "attack-$attack"
        Build-Case $attack 1 $name
        foreach ($memory in @(64, 256)) { Run-Case $name $memory $markers[$attack] }
        if ($attack -eq 3) {
            $serial = [string](Get-Content -LiteralPath (Join-Path $logs "$name-256m-serial.log") -Raw)
            foreach ($spare in 0..2) {
                if (-not $serial.Contains("OOM with spare pages=$spare rejected")) { throw 'An OOM depth was not checked.' }
            }
        }
        if ($attack -eq 5) {
            & (Join-Path $PSScriptRoot 'debug.ps1') -ToolRoot $ToolRoot -Milestone M6 -SkipClearDirection *> (Join-Path $logs 'df-mutation.log')
            Add-Content -LiteralPath $summary -Value 'DF instruction mutation rejected by live C-entry guard'
            Run-Case 'df-normal-retake' 256 $markers[5]
        }
    }
    foreach ($fault in 2..7) {
        $name = "fault-$fault"
        Build-Case 0 $fault $name
        $marker = switch ($fault) {
            2 { 'vector=14 error=0x0000000000000007' }
            3 { 'vector=14 error=0x0000000000000015' }
            4 { 'vector=14 error=0x0000000000000004' }
            5 { 'user exception: task=0 vector=6 error=0x0000000000000000 CS=0x0000000000000023' }
            6 { 'user exception: task=0 vector=13 error=0x0000000000000000 CS=0x0000000000000023' }
            7 { 'user exception: task=0 vector=0 error=0x0000000000000000 CS=0x0000000000000023' }
        }
        foreach ($memory in @(64, 256)) { Run-Case $name $memory $marker }
    }
} finally {
    # Restore the ordinary image even if a fixture failed. The first failure stays in its log.
    Build-Case 0 1 'normal-restored'
}
Run-Case 'normal-restored' 256 'MiniLinux M6: process isolation checks passed'
if (Get-Process -Name qemu-system-x86_64,gdb -ErrorAction SilentlyContinue) { throw 'Guest/debugger remains.' }
if (Get-NetTCPConnection -LocalPort 1234 -State Listen -ErrorAction SilentlyContinue) { throw 'Debug port remains.' }
Add-Content -LiteralPath $summary -Value 'All counterexamples passed; normal image restored; no guest/debugger residue.'
Write-Host 'All counterexamples passed; normal image restored; no guest/debugger residue.'
