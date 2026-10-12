// SWD-14 実装
#include "user_config_apply.h"

namespace cm {
namespace {

uint16_t bit(UserGroup g) {
    return static_cast<uint16_t>(1u << static_cast<uint8_t>(g));
}

/// 1 組を候補に書き込み、通らなければ戻す。何も指定されていない組は触らない
template <typename Fn>
void applyGroup(Config& cfg, UserConfigResult& r, UserGroup g, bool anySet, Fn write) {
    if (!anySet) {
        return;
    }
    const Config before = cfg;
    write(cfg);
    if (validate(cfg, false)) {
        r.appliedMask |= bit(g);
    } else {
        cfg = before;
        r.rejectedMask |= bit(g);
    }
}

template <typename T>
void put(T& dst, const UserValue<T>& v) {
    if (v.set) {
        dst = v.value;
    }
}

}  // namespace

UserConfigResult applyUserConfig(Config& cfg, const UserConfig& u) {
    UserConfigResult r;
    applyGroup(cfg, r, UserGroup::ShowAfr, u.showAfr.set, [&](Config& c) { put(c.showAfr, u.showAfr); });
    applyGroup(cfg, r, UserGroup::DialStyle, u.dialStyle.set,
               [&](Config& c) { put(c.dialStyle, u.dialStyle); });
    applyGroup(cfg, r, UserGroup::Brightness, u.brightness.set,
               [&](Config& c) { put(c.brightness, u.brightness); });
    applyGroup(cfg, r, UserGroup::Stoich, u.stoich.set, [&](Config& c) { put(c.stoich, u.stoich); });

    // 4 つのうち一部だけの指定も受ける（残りは既定値）。単調性はまとめて検査される
    const bool zonesSet = u.richHeavyMax.set || u.richMax.set || u.optimalMax.set || u.leanMax.set;
    applyGroup(cfg, r, UserGroup::Zones, zonesSet, [&](Config& c) {
        put(c.zones.richHeavyMax, u.richHeavyMax);
        put(c.zones.richMax, u.richMax);
        put(c.zones.optimalMax, u.optimalMax);
        put(c.zones.leanMax, u.leanMax);
    });

    applyGroup(cfg, r, UserGroup::Egt, u.egtWarnC.set || u.egtDangerC.set, [&](Config& c) {
        put(c.egt.warnC, u.egtWarnC);
        put(c.egt.dangerC, u.egtDangerC);
    });

    applyGroup(cfg, r, UserGroup::CanBitrate, u.canBitrateKbps.set,
               [&](Config& c) { put(c.canBitrateKbps, u.canBitrateKbps); });
    applyGroup(cfg, r, UserGroup::CanBaseId, u.canBaseId.set,
               [&](Config& c) { put(c.canBaseId, u.canBaseId); });

    cfg.crc32 = computeCrc(cfg);
    return r;
}

const char* userGroupName(UserGroup g) {
    switch (g) {
        case UserGroup::ShowAfr:
            return "show_afr";
        case UserGroup::DialStyle:
            return "dial_style";
        case UserGroup::Brightness:
            return "brightness";
        case UserGroup::Stoich:
            return "stoich";
        case UserGroup::Zones:
            return "zones";
        case UserGroup::Egt:
            return "egt";
        case UserGroup::CanBitrate:
            return "can_bitrate";
        case UserGroup::CanBaseId:
            return "can_base_id";
        default:
            return "?";
    }
}

}  // namespace cm
