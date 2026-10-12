# DOC-40 §4.3: シリアルモニタ (115200bps)。ポートを省くと本体を探す（-Board m5dial で M5Dial）
param([string]$Port, [string]$Board = 'lcd21')
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
if (-not $Port) { $Port = Find-MeterPort -Board $Board }
$pioArgs = @('device', 'monitor', '-b', '115200')
if ($Port) { $pioArgs += @('-p', $Port) }
Invoke-Pio $pioArgs
