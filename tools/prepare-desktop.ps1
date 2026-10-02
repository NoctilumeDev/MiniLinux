param([string]$Proxy = '')
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtime = Join-Path $project 'runtime'
$manifest = Get-Content -LiteralPath (Join-Path $project 'package.json') -Raw | ConvertFrom-Json
function Check-File([string]$Path, [string]$Hash) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing package file: $Path" }
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) { throw "File differs from the package manifest: $Path" }
}
function Download-File([string]$Url, [string]$Path, [string]$Hash) {
    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Host "Downloading $Url"
        $options = @{Uri=$Url; OutFile=($Path + '.part'); UseBasicParsing=$true; TimeoutSec=300}
        if ($Proxy) { $options.Proxy = $Proxy }
        Invoke-WebRequest @options
        Check-File ($Path + '.part') $Hash
        Move-Item -LiteralPath ($Path + '.part') -Destination $Path
    }
    Check-File $Path $Hash
}
Check-File (Join-Path $runtime 'images\minilinux-lab.iso') $manifest.iso_sha256
$cache = Join-Path $runtime 'downloads'
New-Item -ItemType Directory -Path $cache -Force | Out-Null
$pythonDir = Join-Path $runtime 'python'
$qemuDir = Join-Path $runtime 'qemu-20260811'
if (-not (Test-Path -LiteralPath (Join-Path $pythonDir 'python.exe'))) {
    $pythonZip = Join-Path $cache 'python-3.13.16-embed-amd64.zip'
    Download-File 'https://www.python.org/ftp/python/3.13.16/python-3.13.16-embed-amd64.zip' $pythonZip '97dae5274cc54867065e8d5a3226e48c35017ed332a0fdb0e27d5b5821961297'
    Expand-Archive -LiteralPath $pythonZip -DestinationPath $pythonDir -Force
}
if (-not (Test-Path -LiteralPath (Join-Path $qemuDir 'qemu-system-x86_64.exe'))) {
    $qemuArchive = Join-Path $cache 'qemu-w64-setup-20260811.exe'
    Download-File 'https://qemu.weilnetz.de/w64/qemu-w64-setup-20260811.exe' $qemuArchive 'f98a8aeb5f7faea9765b6dee28316c266cd179d80354a2fed8e50176f9a2e59f'
    $extractor = Join-Path $runtime 'extractor'
    Check-File (Join-Path $extractor '7z.exe') $manifest.extractor_exe_sha256
    Check-File (Join-Path $extractor '7z.dll') $manifest.extractor_dll_sha256
    # Read the archive; never run the QEMU installer or write system configuration.
    & (Join-Path $extractor '7z.exe') x $qemuArchive "-o$qemuDir" '-y' 'qemu-system-x86_64.exe' '*.dll' 'share\*' 'lib\*' 'COPYING' 'COPYING.LIB' 'README.rst' 'VERSION' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'QEMU archive extraction failed.' }
}
Check-File (Join-Path $qemuDir 'qemu-system-x86_64.exe') '47d57a6072e0bb3bd98f87926eb129eb1736dfe818c67b3b81ef7ce4edd0b3cd'
Write-Host 'LAB runtime ready. No compiler or debugger is needed.'
