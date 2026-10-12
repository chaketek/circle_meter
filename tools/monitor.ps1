# DOC-40 §4.3: シリアルモニタ (115200bps)。ポートを省くと CH343 を探す
param([string]$Port)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
if (-not $Port) { $Port = Find-MeterPort }
$pioArgs = @('device', 'monitor', '-b', '115200')
if ($Port) { $pioArgs += @('-p', $Port) }
Invoke-Pio $pioArgs
