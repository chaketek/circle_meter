# DOC-40 §4.3 / §2.6: 実機への書き込み
#   .\tools\flash.ps1                 LCD-2.1 実 CAN 版 (500 kbps)
#   .\tools\flash.ps1 -Sim            スイープデモ (CAN なし。SWR-100)
#   .\tools\flash.ps1 -Can1M          CAN 1 Mbps の車両用
#   .\tools\flash.ps1 -Port COM5      ポート指定 (省くと CH343 を探す)
param([string]$Port, [string]$Board = 'lcd21', [switch]$Sim, [switch]$Can1M)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
$envName = Get-MeterEnv -Board $Board -Sim:$Sim -Can1M:$Can1M
if (-not $Port -and $Board -ne 'm5dial') {
    $Port = Find-MeterPort
    if (-not $Port) { throw '本体の COM ポートが見つかりません。UART と書かれた USB-C にデータ通信できるケーブルで繋いでください (docs/user/08_troubleshooting.md)' }
    Write-Host "port: $Port"
}
$pioArgs = @('run', '-e', $envName, '-t', 'upload')
if ($Port) { $pioArgs += @('--upload-port', $Port) }
Invoke-Pio $pioArgs
