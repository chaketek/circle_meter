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
#   -Board m5dial : M5Dial（SWR-68）
function Get-MeterEnv {
    param([string]$Board = 'lcd21', [switch]$Sim, [switch]$Can1M)
    if ($Board -eq 'm5dial') {
        # M5Dial の 1 Mbps は user/user_config.h の USER_CAN_BITRATE_KBPS で指定する（DOC-40 §2.6）
        if ($Can1M) { throw 'M5Dial に -Can1M はありません。user/user_config.h の USER_CAN_BITRATE_KBPS を 1000 にしてください' }
        if ($Sim) { return 'm5dial_sim' } else { return 'm5dial' }
    }
    if ($Sim) { return 'lcd21_sim' }
    if ($Can1M) { return 'lcd21_1m' }
    return 'lcd21'
}

# 本体の COM ポートを探す。見つからなければ $null
#   lcd21  : UART Type-C の CH343（VID 1A86）
#   m5dial : ESP32-S3 のネイティブ USB（VID 303A）
function Find-MeterPort {
    param([string]$Board = 'lcd21')
    $vid = if ($Board -eq 'm5dial') { 'USB\VID_303A*' } else { 'USB\VID_1A86*' }
    $dev = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.PNPDeviceID -like $vid -and $_.Name -match '\((COM\d+)\)' } |
        Select-Object -First 1
    if ($dev -and $dev.Name -match '\((COM\d+)\)') { return $Matches[1] }
    return $null
}
