param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux'
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$llvmBin = Join-Path $ToolRoot 'llvm-23.1.1\clang+llvm-23.1.1-x86_64-pc-windows-msvc\bin'
$clang = Join-Path $llvmBin 'clang.exe'
$linker = Join-Path $llvmBin 'ld.lld.exe'
$readelf = Join-Path $llvmBin 'llvm-readelf.exe'
$build = Join-Path $project 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null

$source = Join-Path $project 'kernel\main.c'
$object = Join-Path $build 'main.o'
$kernel = Join-Path $build 'kernel.elf'
$script = Join-Path $project 'linker.ld'
$includes = Join-Path $project 'include'

& $clang --target=x86_64-unknown-none-elf -std=c11 -Wall -Wextra -Werror `
    -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie `
    -fno-omit-frame-pointer -mno-red-zone -mno-sse -mno-mmx -mno-80387 `
    -mcmodel=kernel -O0 -g -I $includes -c $source -o $object
if ($LASTEXITCODE -ne 0) { throw 'Clang failed.' }

& $linker -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 `
    -T $script -o $kernel $object
if ($LASTEXITCODE -ne 0) { throw 'LLD failed.' }

& $readelf -h $kernel
if ($LASTEXITCODE -ne 0) { throw 'ELF inspection failed.' }
Write-Host "Built $kernel"
