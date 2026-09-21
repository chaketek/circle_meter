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
