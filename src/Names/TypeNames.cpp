/**
 * TypeNames.cpp - Pokemon Type Name Lookup Implementation
 *
 * Maps type IDs to type names.
 * Type IDs match the MoveType enum (0-17):
 * 0=Normal, 1=Fighting, 2=Flying, 3=Poison, 4=Ground, 5=Rock,
 * 6=Bug, 7=Ghost, 8=Steel, 9=Fire, 10=Water, 11=Grass,
 * 12=Electric, 13=Psychic, 14=Ice, 15=Dragon, 16=Dark, 17=Fairy
 */

#include "Names/TypeNames.h"

namespace Names {
    // Type names indexed by type ID (matches MoveType enum)
    static const char* TYPE_NAMES[] = {
        "一般",   // 0 Normal
        "格斗",   // 1 Fighting
        "飞行",   // 2 Flying
        "毒",     // 3 Poison
        "地面",   // 4 Ground
        "岩石",   // 5 Rock
        "虫",     // 6 Bug
        "幽灵",   // 7 Ghost
        "钢",     // 8 Steel
        "火",     // 9 Fire
        "水",     // 10 Water
        "草",     // 11 Grass
        "电",     // 12 Electric
        "超能力", // 13 Psychic
        "冰",     // 14 Ice
        "龙",     // 15 Dragon
        "恶",     // 16 Dark
        "妖精"    // 17 Fairy
    };

    const char* getTypeName(uint8_t typeId) {
        if (typeId < 18) {
            return TYPE_NAMES[typeId];
        }
        return "???";
    }
}
