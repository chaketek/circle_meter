# DOC-40 §4.3: シリアルモニタ (115200bps)
param([string]$Port)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
$pioArgs = @('device', 'monitor', '-b', '115200')
if ($Port) { $pioArgs += @('-p', $Port) }
Invoke-Pio $pioArgs
