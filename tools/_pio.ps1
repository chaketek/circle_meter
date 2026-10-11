# pio が PATH に無ければ python -m platformio へフォールバックする (DOC-40 §2.2)
function Invoke-Pio {
    param([string[]]$PioArgs)
    $exe = Get-Command pio -ErrorAction SilentlyContinue
    if ($exe) {
        & pio @PioArgs
    } else {
        & python -m platformio @PioArgs
    }
    if ($LASTEXITCODE -ne 0) { throw "pio $($PioArgs -join ' ') が失敗しました (exit $LASTEXITCODE)" }
}

# ビルド環境名を決める (DOC-40 §2.6)。既定は LCD-2.1 の実 CAN 版 (500 kbps)
#   -Sim   : CAN の代わりにスイープするデモ (lcd21_sim)
#   -Can1M : CAN 1 Mbps の車両用 (lcd21_1m)
#   -Board m5dial : 廃止予定の M5Dial
function Get-MeterEnv {
    param([string]$Board = 'lcd21', [switch]$Sim, [switch]$Can1M)
    if ($Board -eq 'm5dial') {
        if ($Sim) { return 'm5dial_sim' } else { return 'm5dial' }
    }
    if ($Sim) { return 'lcd21_sim' }
    if ($Can1M) { return 'lcd21_1m' }
    return 'lcd21'
}

# 本体 (LCD-2.1 の CH343 = VID 1A86) の COM ポートを探す。見つからなければ $null
function Find-MeterPort {
    $dev = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.PNPDeviceID -like 'USB\VID_1A86*' -and $_.Name -match '\((COM\d+)\)' } |
        Select-Object -First 1
    if ($dev -and $dev.Name -match '\((COM\d+)\)') { return $Matches[1] }
    return $null
}
