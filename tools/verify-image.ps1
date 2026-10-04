param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux',
    [string]$IsoPath = ''
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$iso = if ($IsoPath) { $IsoPath } else {
    Join-Path $ToolRoot 'images\minilinux-m0.iso'
}
if ((Get-Item -LiteralPath (Join-Path $project 'build') -ErrorAction SilentlyContinue).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw 'Build directory must not be a junction or symlink.'
}
$extract = Join-Path $project ('build\image-check-'+[Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $extract | Out-Null

try {
    & tar.exe -xf $iso -C $extract boot/kernel.elf limine.conf limine-bios.sys
    if ($LASTEXITCODE -ne 0) { throw 'ISO files could not be extracted.' }

    $pairs = @(
        @((Join-Path $extract 'boot\kernel.elf'), (Join-Path $project 'build\kernel.elf')),
        @((Join-Path $extract 'limine.conf'), (Join-Path $project 'boot\limine.conf')),
        @((Join-Path $extract 'limine-bios.sys'), (Join-Path $ToolRoot 'limine-12.9.0\limine-binary\limine-bios.sys'))
    )
    foreach ($pair in $pairs) {
        $inImage = (Get-FileHash -Algorithm SHA256 -LiteralPath $pair[0]).Hash
        $input = (Get-FileHash -Algorithm SHA256 -LiteralPath $pair[1]).Hash
        if ($inImage -ne $input) {
            throw "ISO payload differs from this build: $(Split-Path $pair[0] -Leaf)"
        }
    }
    Write-Host 'ISO kernel, Limine config, and BIOS file match this build.'
}
finally {
    $build = [IO.Path]::GetFullPath((Join-Path $project 'build')) + [IO.Path]::DirectorySeparatorChar
    $resolved = (Resolve-Path -LiteralPath $extract).Path
    if (-not $resolved.StartsWith($build, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Extraction cleanup escaped the build directory.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
