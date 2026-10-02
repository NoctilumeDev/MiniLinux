param([Parameter(Mandatory=$true)][ValidateSet('baseline', 'fixture')][string]$Mode)
$ErrorActionPreference = 'Stop'
if (Get-Process -Name qemu-system-x86_64,gdb -ErrorAction SilentlyContinue) {
    throw 'An existing QEMU/GDB process prevents this sequential probe run.'
}
& 'D:\python-3.10.6\python.exe' (Join-Path $PSScriptRoot 'probe-supplemental.py') --mode $Mode
if ($LASTEXITCODE -ne 0) { throw "Supplemental $Mode probe failed." }
if (Get-Process -Name qemu-system-x86_64,gdb -ErrorAction SilentlyContinue) {
    throw 'Guest/debugger remained after the probe run.'
}
