# DOC-40 §4.4: 書き込み後そのままログを見る
param([string]$Port, [switch]$Sim)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/flash.ps1" -Port $Port -Sim:$Sim
& "$PSScriptRoot/monitor.ps1" -Port $Port
