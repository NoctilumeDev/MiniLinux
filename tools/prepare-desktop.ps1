param([string]$Proxy = '')
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtime = Join-Path $project 'runtime'
$manifest = Get-Content -LiteralPath (Join-Path $project 'package.json') -Raw | ConvertFrom-Json
Write-Host 'First start downloads about 220 MB of official archives; later starts use this folder cache.'
Write-Host 'Runtime stays in this package. No system installation or global proxy setting is changed.'
function Check-File([string]$Path, [string]$Hash) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing package file: $Path" }
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) { throw "File differs from the package manifest: $Path" }
}
function Download-File([string]$Url, [string]$Path, [string]$Hash) {
    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Host "Downloading $Url"
        Write-Host 'Downloading now; the first run can take several minutes. Failed downloads can be retried.'
        $options = @{Uri=$Url; OutFile=($Path + '.part'); UseBasicParsing=$true; TimeoutSec=300}
        if ($Proxy) { $options.Proxy = $Proxy }
        Invoke-WebRequest @options
        Check-File ($Path + '.part') $Hash
        Move-Item -LiteralPath ($Path + '.part') -Destination $Path
    }
    else { Write-Host "Using cached archive: $([IO.Path]::GetFileName($Path))" }
    Check-File $Path $Hash
}
Write-Host '[1/4] Checking the packaged kernel image'
Check-File (Join-Path $runtime 'images\minilinux-lab.iso') $manifest.iso_sha256
$cache = Join-Path $runtime 'downloads'
New-Item -ItemType Directory -Path $cache -Force | Out-Null
$pythonDir = Join-Path $runtime 'python'
$qemuDir = Join-Path $runtime 'qemu-20260811'
Write-Host '[2/4] Preparing the embedded Python runtime'
if (-not (Test-Path -LiteralPath (Join-Path $pythonDir 'python.exe'))) {
    $pythonZip = Join-Path $cache 'python-3.13.16-embed-amd64.zip'
    Download-File 'https://www.python.org/ftp/python/3.13.16/python-3.13.16-embed-amd64.zip' $pythonZip '97dae5274cc54867065e8d5a3226e48c35017ed332a0fdb0e27d5b5821961297'
    Expand-Archive -LiteralPath $pythonZip -DestinationPath $pythonDir -Force
}
else { Write-Host 'Using this package Python runtime.' }
Write-Host '[3/4] Preparing QEMU (download, verify, then unpack)'
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
else { Write-Host 'Using this package QEMU runtime.' }
Write-Host '[4/4] Checking the QEMU runtime; LAB will open next'
Check-File (Join-Path $qemuDir 'qemu-system-x86_64.exe') '47d57a6072e0bb3bd98f87926eb129eb1736dfe818c67b3b81ef7ce4edd0b3cd'
Write-Host 'LAB runtime ready. Keep Start.cmd open; Ctrl+C closes LAB and its QEMU.'
