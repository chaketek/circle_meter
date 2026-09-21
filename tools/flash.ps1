# DOC-40 §4.3: 実機への書き込み
#   .\tools\flash.ps1                 通常版
#   .\tools\flash.ps1 -Sim            CAN シミュレータ版 (SWR-100)
#   .\tools\flash.ps1 -Port COM5      ポート指定
param([string]$Port, [switch]$Sim)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
$envName = if ($Sim) { 'm5dial_sim' } else { 'm5dial' }
$pioArgs = @('run', '-e', $envName, '-t', 'upload')
if ($Port) { $pioArgs += @('--upload-port', $Port) }
Invoke-Pio $pioArgs
