# DOC-40 §4.3: ドメイン層の単体テスト (SWE.4 / UT-*)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
. "$PSScriptRoot/_pio.ps1"
Invoke-Pio @('test', '-e', 'native')
