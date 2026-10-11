# DOC-40 §4.3 / §2.6: ローカルビルド
#   .\tools\build.ps1            LCD-2.1 実 CAN 版 (500 kbps)
#   .\tools\build.ps1 -Sim       スイープデモ (CAN なし)
#   .\tools\build.ps1 -Can1M     CAN 1 Mbps の車両用
param([string]$Board = 'lcd21', [switch]$Sim, [switch]$Can1M)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
$envName = Get-MeterEnv -Board $Board -Sim:$Sim -Can1M:$Can1M
Invoke-Pio @('run', '-e', $envName)
