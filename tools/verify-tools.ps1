param(
    [string]$ToolRoot = 'D:\DevTools\MiniLinux'
)

$ErrorActionPreference = 'Stop'
$llvmBin = Join-Path $ToolRoot 'llvm-23.1.1\clang+llvm-23.1.1-x86_64-pc-windows-msvc\bin'
$checks = @(
    @('Clang', (Join-Path $llvmBin 'clang.exe'), 'D43B7FA07B5B77B716E60600AD2792CFB2EEB370AECB6144BF6C698F2C6D7467'),
    @('LLD', (Join-Path $llvmBin 'ld.lld.exe'), '09F880791C158E4784F603078AF102B2AEB0F18DE551255977B81343385EFC95'),
    @('ELF inspector', (Join-Path $llvmBin 'llvm-readelf.exe'), '7F51B89AB673FC89712D6E48C215E9B1CD8FF6B2B46DBBE11CD85574C6A61224'),
    @('QEMU', (Join-Path $ToolRoot 'qemu-20260811\qemu-system-x86_64.exe'), '47D57A6072E0BB3BD98F87926EB129EB1736DFE818C67B3B81EF7CE4EDD0B3CD'),
    @('Limine', (Join-Path $ToolRoot 'limine-12.9.0\limine-binary\limine-tool-windows-x86\limine.exe'), '874D267CADFBE778F830D7A78C01E39AA3AF98D55D2D069882062ECFA464F5E6'),
    @('Limine BIOS CD', (Join-Path $ToolRoot 'limine-12.9.0\limine-binary\limine-bios-cd.bin'), '0A0F509CD2E8B0F7EA4CFCA6EAC5B88A4AC477CD102CC90FB7E2C21BA80B2A22'),
    @('Limine BIOS stage', (Join-Path $ToolRoot 'limine-12.9.0\limine-binary\limine-bios.sys'), '8E432DB7B4721F906C9136B3854D3D9FEC7B337D6F6D049C0F2A1BA8EA591507'),
    @('GDB', 'D:\GCC\mingw64\bin\gdb.exe', '5821C8742377D6B9823C4C1F039C858F5410239E154DB07A5FB80A52CDCCB32D'),
    @('Python', 'D:\python-3.10.6\python.exe', '32CE1D2650EA8B9D394F5B8F94677D27888DCCDC3713365BF903A8C465C9D776')
)

foreach ($check in $checks) {
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $check[1]).Hash
    if ($actual -ne $check[2]) {
        throw "$($check[0]) differs from the M0 tool baseline: $($check[1])"
    }
}

$env:PYTHONPATH = Join-Path $ToolRoot 'python-packages'
$version = & 'D:\python-3.10.6\python.exe' -c 'import importlib.metadata; print(importlib.metadata.version("pycdlib"))'
if ($LASTEXITCODE -ne 0 -or $version.Trim() -ne '1.20.0') {
    throw 'pycdlib differs from the M0 baseline.'
}
Write-Host 'M0 tool digests and pycdlib version match the baseline.'
