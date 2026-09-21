# DOC-30 §3: PCAN ベンチで CAN パターンを流し、表示を目視確認する
#   .\tools\bench_check.ps1                       実走模擬を 45 秒
#   .\tools\bench_check.ps1 -Seconds 20
#   .\tools\bench_check.ps1 -Mode sweep           青ゾーンも見たい
#   .\tools\bench_check.ps1 -Mode egt-danger      QT-05 警告確認
#   .\tools\bench_check.ps1 -Listen               IT-04 本機が送信しないこと
#   .\tools\bench_check.ps1 -Flash                先にビルドして書き込む
param(
    [ValidateSet('drive','idle','sweep','egt-danger','dropout','burst','invalid')]
    [string]$Mode = 'drive',
    [double]$Seconds = 45,
    [string]$Port,
    [string]$Channel = 'PCAN_USBBUS1',
    [switch]$Flash,
    [switch]$Sim,
    [switch]$Listen
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$a = @('tools/bench_check.py', '--mode', $Mode, '--seconds', $Seconds, '--channel', $Channel)
if ($Port)   { $a += @('--port', $Port) }
if ($Flash)  { $a += '--flash' }
if ($Sim)    { $a += '--sim' }
if ($Listen) { $a += '--listen' }
& python @a
