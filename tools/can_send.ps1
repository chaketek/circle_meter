# DOC-30 §3: PCAN から rusEFI verbose broadcast を模擬送出する（結合テスト IT-*）
#   .\tools\can_send.ps1                      sweep（λ を往復させる）
#   .\tools\can_send.ps1 -Mode idle
#   .\tools\can_send.ps1 -Mode dropout        IT-02 途絶検出
#   .\tools\can_send.ps1 -Mode burst          IT-03 取りこぼし
#   .\tools\can_send.ps1 -Mode invalid        UT-09 の実機確認
#   .\tools\can_send.ps1 -Mode egt-danger     QT-05 危険警告
#   .\tools\can_send.ps1 -Listen 10           IT-04 本機が送信しないことの確認
param(
    [ValidateSet('idle','sweep','egt-danger','dropout','burst','invalid','replay')]
    [string]$Mode = 'sweep',
    [string]$Channel = 'PCAN_USBBUS1',
    [int]$Bitrate = 500000,
    [string]$Base = '0x200',
    [int]$PeriodMs = 50,
    [string]$Csv,
    [double]$Listen
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$a = @('tools/pcan_send.py', '--mode', $Mode, '--channel', $Channel,
       '--bitrate', $Bitrate, '--base', $Base, '--period-ms', $PeriodMs)
if ($Csv)    { $a += @('--csv', $Csv) }
if ($Listen) { $a += @('--listen', $Listen) }
& python @a
