// SWA-02: CAN 受信タスク / SWA-01 の健全性監視タスク
//
// M5Dial 版の src/main.cpp に埋め込まれていた処理を lib/hal に切り出したもの。
// M5Dial 版（src/main.cpp）は廃止予定なので、そちらは触っていない（二重実装は P4 完了時に解消する）。
#pragma once

#include "can_driver.h"
#include "config.h"
#include "signal_store.h"

namespace cm {

/// Core 0 に固定した受信タスクを起動する（DEC-03）。受信 -> デコード -> SignalStore。
/// 引数の参照はタスクが動いている間ずっと有効でなければならない（静的領域に置くこと）。
void startCanRxTask(CanDriver& can, SignalStore& store, const Config& cfg);

/// 1 秒周期でバスオフ検出・復旧・統計更新（CanDriver::poll）を行うタスク。
void startCanHealthTask(CanDriver& can);

}  // namespace cm
