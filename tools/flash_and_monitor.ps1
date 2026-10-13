# DOC-40 §4.4: 書き込み後そのままログを見る
param([string]$Port, [string]$Board = 'lcd21', [switch]$Sim, [switch]$Can1M)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/flash.ps1" -Port $Port -Board $Board -Sim:$Sim -Can1M:$Can1M
& "$PSScriptRoot/monitor.ps1" -Port $Port -Board $Board
