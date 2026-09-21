# 13. インタフェース制御文書 — rusEFI CAN (ICD)

| 項目 | 内容 |
|---|---|
| 文書ID | `DOC-13` |
| プロセス | SYS.3（外部インタフェース定義） |
| 版 | 0.1 (Draft) |
| 最終更新 | 2026-09-21 |
| 一次情報源 | rusEFI `firmware/controllers/can/can_verbose.cpp` および `rusEFI_CAN_verbose.dbc`（master, 2026-09 時点） |

---

## 1. 物理層・データリンク層

| 項目 | 値 | 備考 |
|---|---|---|
| 規格 | ISO 11898-2 High-Speed CAN | |
| ビットレート | **500 kbps**（既定） | rusEFI `canBaudRate` と一致させること |
| ID 形式 | **11 bit 標準 ID**（既定） | rusEFI の `rusefiVerbose29b` を有効にすると 29 bit 拡張 ID になる。本機は両対応とし設定で切替 |
| 全フレーム DLC | 8 バイト固定 | |
| バイトオーダ | **リトルエンディアン (Intel)** | DBC の `@1` 指定 |
| 終端抵抗 | 車両側で 2 本済み。**本機は追加しない** | `SYS-52`, `RSK-02` |
| 本機の送信 | **なし**（Listen Only） | `SYS-07` |

## 2. フレーム一覧（verbose broadcast）

ベース ID は rusEFI の `verboseCanBaseAddress`。**既定 `0x200` (512)**。

| オフセット | ID (既定) | 名称 | 本機での用途 | 優先度 |
|---|---|---|---|---|
| BASE+0 | `0x200` | Status | 状態フラグ・ギア・積算距離 | 副 |
| BASE+1 | `0x201` | Speeds | RPM・点火時期・噴射/点火デューティ・車速 | 副 |
| BASE+2 | `0x202` | PedalAndTps | ペダル・スロットル開度 | 副 |
| BASE+3 | `0x203` | Sensors1 | MAP・水温・吸気温・燃料残量 | 副 |
| BASE+4 | `0x204` | Sensors2 | 油圧・油温・燃温・バッテリ電圧 | 副 |
| BASE+5 | `0x205` | Fueling | 空気量・噴射時間・ノックカウント | 未使用 |
| BASE+6 | `0x206` | Fueling2 | 燃料消費・燃料補正 | 未使用 |
| **BASE+7** | **`0x207`** | **Fueling3** | **λ1（主表示）** | **★ Must** |
| BASE+8 | `0x208` | Cams | VVT 位置 | 未使用（NA ロードスターは非搭載） |
| **BASE+9** | **`0x209`** | **Egts** | **EGT1（主表示）** | **★ Must** |
| BASE+10 | `0x20A` | PerCylinderKnock | 気筒別ノック | 未使用 |
| BASE+11 | `0x20B` | Status11 | ブレーキペダル | 未使用 |

## 3. 主要フレーム詳細

### 3.1 `0x207` Fueling3 — λ（主表示・必須）

| 信号 | バイト | ビット | 型 | 係数 | オフセット | 単位 | 範囲 |
|---|---|---|---|---|---|---|---|
| **Lam1** | 0–1 | 0\|16 | uint16 LE | **0.0001** | 0 | λ | 0 – 6.5535 |
| Lam2 | 2–3 | 16\|16 | uint16 LE | 0.0001 | 0 | λ | — |
| FpLow | 4–5 | 32\|16 | uint16 LE | 1/30 | 0 | kPa | — |
| FpHigh | 6–7 | 48\|16 | uint16 LE | 0.1 | 0 | bar | — |

```
λ = (uint16)(data[0] | (data[1] << 8)) × 0.0001
```

**無効値の扱い**: rusEFI は `Sensor::getOrZero()` を用いるため、センサ未構成・未ウォームアップ時は **0** が送られる。
`λ < 0.30` を無効値として扱い、表示を `--` とする（`SYS-42`）。実用上 λ が 0.30 を下回ることはない。

### 3.2 `0x209` Egts — 排気温度（主表示・必須）

| 信号 | バイト | ビット | 型 | 係数 | 単位 | 範囲 |
|---|---|---|---|---|---|---|
| **Egt1** | 0 | 0\|8 | uint8 | **5** | °C | 0 – 1275 |
| Egt2 | 1 | 8\|8 | uint8 | 5 | °C | 0 – 1275 |
| Egt3–Egt8 | 2–7 | — | uint8 | 5 | °C | **rusEFI 本体は現状 Egt1/Egt2 のみ送信。3–8 は常に 0** |

```
EGT[°C] = data[0] × 5
```

**分解能に関する注意**: 5 °C/LSB であるため、表示上の最小変化幅は 5 °C になる。
これを滑らかに見せるため、表示側で 1 次ローパス（時定数 200 ms）を掛ける（`SWR-24`）。

**無効値の扱い**: 未構成時は 0 が送られる。`EGT ≤ 0` を無効として `--` 表示（`RSK-09`）。

### 3.3 `0x201` Speeds — 副表示

| 信号 | バイト | ビット | 型 | 係数 | オフセット | 単位 |
|---|---|---|---|---|---|---|
| RPM | 0–1 | 0\|16 | uint16 LE | 1 | 0 | rpm |
| IgnitionTiming | 2–3 | 16\|16 | **int16 LE** | 0.02 | 0 | deg |
| InjDuty | 4 | 32\|8 | uint8 | 0.5 | 0 | % |
| IgnDuty | 5 | 40\|8 | uint8 | 0.5 | 0 | % |
| VehicleSpeed | 6 | 48\|8 | uint8 | 1 | 0 | km/h |
| FlexPct | 7 | 56\|8 | uint8 | 1 | 0 | % |

### 3.4 `0x203` Sensors1 — 副表示

| 信号 | バイト | ビット | 型 | 係数 | オフセット | 単位 |
|---|---|---|---|---|---|---|
| MAP | 0–1 | 0\|16 | uint16 LE | 1/30 | 0 | kPa |
| CoolantTemp | 2 | 16\|8 | uint8 | 1 | **−40** | °C |
| IntakeTemp | 3 | 24\|8 | uint8 | 1 | **−40** | °C |
| AUX1Temp | 4 | 32\|8 | uint8 | 1 | −40 | °C |
| AUX2Temp | 5 | 40\|8 | uint8 | 1 | −40 | °C |
| MCUTemp | 6 | 48\|8 | uint8 | 1 | −40 | °C |
| FuelLevel | 7 | 56\|8 | uint8 | 0.5 | 0 | % |

### 3.5 `0x204` Sensors2 — 副表示

| 信号 | バイト | ビット | 型 | 係数 | オフセット | 単位 |
|---|---|---|---|---|---|---|
| (予約) | 0–1 | — | — | — | — | 未使用 |
| OilPress | 2–3 | 16\|16 | uint16 LE | 1/30 | 0 | kPa |
| OilTemperature | 4 | 32\|8 | uint8 | 1 | −40 | °C |
| FuelTemperature | 5 | 40\|8 | uint8 | 1 | −40 | °C |
| BattVolt | 6–7 | 48\|16 | uint16 LE | 0.001 | 0 | V |

### 3.6 `0x200` Status — 副表示・警告灯

| 信号 | バイト | ビット位置 | 型 | 意味 |
|---|---|---|---|---|
| WarningCounter | 0–1 | 0\|16 | uint16 LE | 起動後の警告累計 |
| LastError | 2–3 | 16\|16 | uint16 LE | 最終エラーコード |
| RevLimAct | 4 | bit 0 | bool | レブリミッタ作動 |
| MainRelayAct | 4 | bit 1 | bool | メインリレー |
| FuelPumpAct | 4 | bit 2 | bool | 燃料ポンプ |
| CELAct | 4 | bit 3 | bool | **チェックエンジン灯** |
| EGOHeatAct | 4 | bit 4 | bool | O2 ヒータ |
| LambdaProtectAct | 4 | bit 5 | bool | **λ プロテクト作動** |
| Fan | 4 | bit 6 | bool | ファン 1 |
| Fan2 | 4 | bit 7 | bool | ファン 2 |
| CurrentGear | 5 | 40\|8 | uint8 | 検出ギア |
| DistanceTraveled | 6–7 | 48\|16 | uint16 LE | 0.1 km 単位 |

## 4. タイミング

| 項目 | 値 | 根拠 |
|---|---|---|
| verbose broadcast 周期 | rusEFI `canSleepPeriodMs` の設定値。実効周期は 5/10/20/50/100/200/250/500/1000 ms のいずれかに丸められる | `can_tx.cpp: roundTxPeriodToCycle()` |
| 想定周期 | **50 ms (20 Hz)** | rusEFI の一般的な既定値。`OPN-03` で実設定を確認 |
| 1 周期あたりの送信フレーム数 | 12 フレーム（BASE+0 〜 BASE+11） | `sendCanVerbose()` |
| バス占有率（概算） | 12 frames × (約 108 bit + スタッフ) ÷ 50 ms ÷ 500 kbps ≒ **5.2 %** | — |
| 鮮度切れ判定 | **500 ms**（想定周期の 10 倍） | `SYS-40` |
| 信号喪失判定 | **2000 ms** | `SYS-41` |

> **注意**: シリアル over CAN 使用時は rusEFI 側で周期が 5 倍に伸びる（`pauseCANdueToSerial`）。
> TunerStudio を CAN 経由で接続している場合、周期は最大 250 ms 相当まで劣化しうる。
> 鮮度切れ判定 500 ms はこの条件では誤検出する可能性があるため、
> ベンチ試験で確認し必要なら閾値を調整する（`IT-02`）。

## 5. ECU 側の前提設定（車両で確認すべき項目）

| rusEFI 設定 | 必要な値 | 確認方法 |
|---|---|---|
| `enableVerboseCanTx` | **true** | TunerStudio → CAN bus 設定 |
| `canWriteEnabled` | **true** | 同上 |
| `verboseCanBaseAddress` | 0x200（既定）または本機と一致する値 | 同上 |
| `rusefiVerbose29b` | false（11 bit 推奨） | 同上 |
| `canBaudRate` | 500 kbps | 同上 |
| `canBroadcastUseChannel` | 本機を接続するチャンネル | 同上 |
| `Lambda1` センサ | ワイドバンドコントローラから構成済み | TunerStudio ゲージで値が出ること |
| `EGT1` センサ | EGT アンプ（MAX31855 / CAN-EGT 等）から構成済み | 同上 |

## 6. 代替インタフェース（フォールバック）

verbose broadcast が使えない場合の代替として、rusEFI は OBD-II 応答（要求 `0x7DF`、応答 `0x7E8`）にも対応する。
ただし **λ と EGT は標準 PID に存在しない**ため本件の Must 要求を満たせない。
よって本機は verbose broadcast のみを実装し、OBD-II は採用しない。

## 7. 変更管理

rusEFI 側の CAN 仕様が変更された場合、本 ICD と `lib/rusefi_can/` のデコーダを同時に更新する。
参照した rusEFI のコミットハッシュを `lib/rusefi_can/include/rusefi_can_spec.h` の冒頭コメントに記録し、
互換性の追跡点とする（`RSK-08`）。
