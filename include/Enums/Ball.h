#ifndef ENUMS_BALL_H
#define ENUMS_BALL_H

#include <cstdint>
#include <cstddef>
#include <vector>

#include "Enums/GameVersion.h"

namespace Enums {
    /// Ball IDs for the corresponding English ball name.
    enum class Ball
    {
        None,
        Master,
        Ultra,
        Great,
        Poke,
        Safari,
        Net,
        Dive,
        Nest,
        Repeat,
        Timer,
        Luxury,
        Premier,
        Dusk,
        Heal,
        Quick,
        Cherish,
        Fast,
        Level,
        Lure,
        Heavy,
        Love,
        Friend,
        Moon,
        Sport,
        Dream,
        Beast,
        // Legends: Arceus
        Strange,
        LAPoke,
        LAGreat,
        LAUltra,
        LAFeather,
        LAWing,
        LAJet,
        LAHeavy,
        LALeaden,
        LAGigaton,
        LAOrigin
    };

    /// English ball name for a stored ball id (indices match the Ball enum above).
    inline const char* getBallName(uint8_t id) {
        static const char* const names[] = {
            "（无）", "大师球", "高级球", "超级球", "精灵球", "狩猎球",
            "捕网球", "潜水球", "巢穴球", "重复球", "计时球", "豪华球",
            "纪念球", "黑暗球", "治愈球", "先机球", "贵重球", "速度球",
            "等级球", "诱饵球", "沉重球", "甜蜜球", "友友球", "月亮球",
            "竞赛球", "梦境球", "究极球", "奇异球", "精灵球", "超级球",
            "高级球", "飞羽球", "飞翼球", "飞梭球", "沉重球", "超重球",
            "巨重球", "起源球"
        };
        return id < (sizeof(names) / sizeof(names[0])) ? names[id] : "（未知）";
    }

    /// The ball ids a given game can actually use, for the per-game ball picker. Legends: Arceus is the
    /// odd one out -- it stores its own Hisui ball set (Strange + LA Poke..Origin, ids 27-37) and none of
    /// the standard balls; the standard games use ids 1-26 (Gen 3 / Let's Go carry a smaller subset).
    inline std::vector<uint8_t> getBallList(GameVersion group) {
        switch (group) {
            case GameVersion::PLA:  // Hisui: Strange (transfers) + Poke/Great/Ultra/Feather..Origin
                return {27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37};
            case GameVersion::FRLG: // Gen 3 balls (Master..Premier)
                return {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
            case GameVersion::GG:   // Let's Go: Master, Ultra, Great, Poke, Premier only
                return {1, 2, 3, 4, 12};
            default:                // SwSh / BDSP / SV / Z-A: the standard modern set
                return {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                        14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
        }
    }
}

#endif
