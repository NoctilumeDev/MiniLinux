param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [ValidateSet('M0', 'M1', 'M2', 'M3', 'M4', 'M5', 'M6', 'LAB')][string]$Milestone = 'M0',
    [ValidateRange(1, 7)][int]$FaultCase = 1,
    [ValidateRange(0, 5)][int]$AttackCase = 0
)

$ErrorActionPreference = 'Stop'
if ($Milestone -ne 'M6' -and $FaultCase -ne 1) { throw 'Additional protection probes belong to M6.' }
if ($Milestone -ne 'M6' -and $AttackCase -ne 0) { throw 'Counterexamples belong to M6.' }
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$llvmBin = Join-Path $ToolRoot 'llvm-23.1.1\clang+llvm-23.1.1-x86_64-pc-windows-msvc\bin'
$clang = Join-Path $llvmBin 'clang.exe'
$linker = Join-Path $llvmBin 'ld.lld.exe'
$readelf = Join-Path $llvmBin 'llvm-readelf.exe'
$build = Join-Path $project 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null

$sources = @('kernel\main.c')
$level = if ($Milestone -eq 'LAB') { 7 } else { [int]$Milestone.Substring(1) }
$defines = @("-DMINILINUX_LEVEL=$level", "-DMINILINUX_PROBE=$FaultCase", "-DMINILINUX_ATTACK=$AttackCase")
if ($AttackCase -ne 0) { $sources += 'kernel\counterexample.c' }
if ($level -ge 1) {
    $sources += @('kernel\memory\page.c', 'kernel\memory\page_test.c')
}
if ($level -ge 2) {
    $sources += @('kernel\bytes.c', 'kernel\memory\vm.c', 'kernel\memory\vm_test.c')
}
if ($level -ge 3) {
    $sources += @('kernel\cpu.c', 'kernel\interrupt.c', 'kernel\entry.S')
    if ($level -ne 7) { $sources += 'kernel\task.c' }
}
if ($level -ge 4) {
    $userObject = Join-Path $build 'user-program.o'
    $userElf = Join-Path $build 'user.elf'
    $userBinary = Join-Path $build 'user.bin'
    $userSource = if ($level -eq 7) { 'user\laboratory.c' } else { 'user\program.c' }
    & $clang --target=x86_64-unknown-none-elf -std=c11 -Wall -Wextra -Werror `
        -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie `
        -fno-omit-frame-pointer -mno-red-zone -mno-sse -mno-mmx -mno-80387 `
        -mcmodel=small -O0 -g -I (Join-Path $project 'include') @defines `
        -c (Join-Path $project $userSource) -o $userObject
    if ($LASTEXITCODE -ne 0) { throw 'User program compilation failed.' }
    $userObjects = @($userObject)
    if ($level -eq 7) {
        # Aggregate initialization can emit memset even in freestanding C.
        # Compile the same plain byte helpers for the user address space.
        $userBytes = Join-Path $build 'user-bytes.o'
        & $clang --target=x86_64-unknown-none-elf -std=c11 -Wall -Wextra -Werror `
            -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie `
            -mno-red-zone -mno-sse -mno-mmx -mno-80387 -mcmodel=small -O0 -g `
            -I (Join-Path $project 'include') -c (Join-Path $project 'kernel\bytes.c') -o $userBytes
        if ($LASTEXITCODE -ne 0) { throw 'User byte helpers failed.' }
        $userObjects += $userBytes
    }
    & $linker -m elf_x86_64 -nostdlib -static -T (Join-Path $project 'user\linker.ld') -o $userElf @userObjects
    if ($LASTEXITCODE -ne 0) { throw 'User program link failed.' }
    & (Join-Path $llvmBin 'llvm-objcopy.exe') -O binary $userElf $userBinary
    if ($LASTEXITCODE -ne 0) { throw 'User binary extraction failed.' }
    $bytes = [IO.File]::ReadAllBytes($userBinary)
    $codeLimit = if ($level -eq 7) { 65536 } else { 8192 }
    if ($bytes.Length -eq 0 -or $bytes.Length -gt $codeLimit) { throw 'User image exceeds its fixed code page limit.' }
    $values = ($bytes | ForEach-Object { '0x{0:x2}' -f $_ }) -join ','
    $embedded = Join-Path $build 'user-image.c'
    Set-Content -LiteralPath $embedded -Value @('#include <stdint.h>', "const unsigned char user_image[] = {$values};", "const uint64_t user_image_size = $($bytes.Length);")
    $sources += @('kernel\usercopy.c', 'build\user-image.c')
    if ($level -eq 7) { $sources += 'kernel\laboratory.c' }
    else { $sources += @('kernel\user.c', 'kernel\syscall.c') }
}
if ($level -ge 5) {
    $sources += 'kernel\ramfs.c'
}
$objects = @()
$kernel = Join-Path $build 'kernel.elf'
$script = Join-Path $project 'linker.ld'
$includes = Join-Path $project 'include'

foreach ($relative in $sources) {
    $source = Join-Path $project $relative
    $object = Join-Path $build ([IO.Path]::GetFileNameWithoutExtension($relative) + '.o')
    $language = @(if ($relative.EndsWith('.c')) { '-std=c11' })
    & $clang --target=x86_64-unknown-none-elf @language -Wall -Wextra -Werror `
        -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie `
        -fno-omit-frame-pointer -mno-red-zone -mno-sse -mno-mmx -mno-80387 `
        -mcmodel=kernel -O0 -g -I $includes @defines -c $source -o $object
    if ($LASTEXITCODE -ne 0) { throw "Clang failed: $relative" }
    $objects += $object
}

& $linker -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 `
    -T $script -o $kernel @objects
if ($LASTEXITCODE -ne 0) { throw 'LLD failed.' }

& $readelf -h $kernel
if ($LASTEXITCODE -ne 0) { throw 'ELF inspection failed.' }
Write-Host "Built $kernel"
