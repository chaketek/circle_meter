# DOC-40 §4.3: ローカルビルド
param([switch]$Sim)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
$envName = if ($Sim) { 'm5dial_sim' } else { 'm5dial' }
Invoke-Pio @('run', '-e', $envName)
