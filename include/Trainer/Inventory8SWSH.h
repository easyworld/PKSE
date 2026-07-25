#ifndef TRAINER_INVENTORY8_H
#define TRAINER_INVENTORY8_H

#include <cstdint>

#include "Trainer/Inventory.h"

namespace Trainer {
    struct InventoryItem8SWSH : InventoryItem {
        // Decode from 32-bit value
        static InventoryItem8SWSH fromValue(uint32_t value) {
            InventoryItem8SWSH item;
            item.itemId = value & 0x7FF;
            item.count = (value >> 15) & 0x3FF;
            item.isNew = (value & 0x40000000) != 0;
            item.isFavorite = (value & 0x80000000) != 0;
            return item;
        }

        // Encode to 32-bit value
        uint32_t toValue() const {
            uint32_t val = (itemId & 0x7FF) | ((count & 0x3FF) << 15);
            if (isNew) val |= 0x40000000;
            if (isFavorite) val |= 0x80000000;
            return val;
        }
    };

    /// Item pouch categories
    enum class PouchType8SWSH {
        Medicine = 0,   // Offset 0, 60 slots
        Balls,          // Offset 240, 30 slots
        Battle,         // Offset 360, 20 slots
        Berries,        // Offset 440, 80 slots
        Items,          // Offset 760, 550 slots
        TMs,            // Offset 2960, 210 slots
        Treasures,      // Offset 3800, 100 slots
        Ingredients,    // Offset 4200, 100 slots
        KeyItems,       // Offset 4600, 64 slots
        Count           // Total number of pouches
    };

    /// Pouch information
    struct PouchInfo8SWSH {
        PouchType8SWSH type;
        const char* name;
        int offset;
        int maxCount;
    };

    /// Get pouch info for a given type
    inline const PouchInfo8SWSH& getPouchInfo8SWSH(PouchType8SWSH type) {
        static const PouchInfo8SWSH pouches[] = {
            {PouchType8SWSH::Medicine, "回复道具", 0, 60},
            {PouchType8SWSH::Balls, "精灵球", 240, 30},
            {PouchType8SWSH::Battle, "对战道具", 360, 20},
            {PouchType8SWSH::Berries, "树果", 440, 80},
            {PouchType8SWSH::Items, "其他道具", 760, 550},
            {PouchType8SWSH::TMs, "招式学习器", 2960, 210},
            {PouchType8SWSH::Treasures, "宝物", 3800, 100},
            {PouchType8SWSH::Ingredients, "食材", 4200, 100},
            {PouchType8SWSH::KeyItems, "重要道具", 4600, 64}
        };
        return pouches[static_cast<int>(type)];
    }
}

#endif
