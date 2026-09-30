#ifndef TRAINER_INVENTORY8_H
#define TRAINER_INVENTORY8_H

#include <cstdint>

#include "Trainer/Inventory.h"

namespace Trainer
{
    struct InventoryItem8SWSH : InventoryItem
    {
        // Decode from 32-bit value
        static InventoryItem8SWSH fromValue(uint32_t value)
        {
            InventoryItem8SWSH item;
            item.itemId = value & 0x7FF;
            item.count = (value >> 15) & 0x3FF;
            item.isNew = (value & 0x40000000) != 0;
            item.isFavorite = (value & 0x80000000) != 0;
            return item;
        }

        // Encode to 32-bit value
        uint32_t toValue() const
        {
            uint32_t value = (itemId & 0x7FF) | ((count & 0x3FF) << 15);
            if (isNew)
                value |= 0x40000000;
            if (isFavorite)
                value |= 0x80000000;
            return value;
        }
    };

    /// Item pouch categories
    enum class PouchType8SWSH
    {
        Medicine = 0, // Offset 0, 60 slots
        Balls,        // Offset 240, 30 slots
        Battle,       // Offset 360, 20 slots
        Berries,      // Offset 440, 80 slots
        Items,        // Offset 760, 550 slots
        TMs,          // Offset 2960, 210 slots
        Treasures,    // Offset 3800, 100 slots
        Ingredients,  // Offset 4200, 100 slots
        KeyItems,     // Offset 4600, 64 slots
        Count         // Total number of pouches
    };

    struct PouchInfo8SWSH
    {
        PouchType8SWSH type;
        const char *name;
        int offset;
        int maxCount;
    };

    inline const PouchInfo8SWSH &getPouchInfo8SWSH(PouchType8SWSH type)
    {
        static const PouchInfo8SWSH pouches[] = {
            {PouchType8SWSH::Medicine, "Medicines", 0, 60},
            {PouchType8SWSH::Balls, "Poké Balls", 240, 30},
            {PouchType8SWSH::Battle, "Battle Items", 360, 20},
            {PouchType8SWSH::Berries, "Berries", 440, 80},
            {PouchType8SWSH::Items, "Other Items", 760, 550},
            {PouchType8SWSH::TMs, "TMs/TRs", 2960, 210},
            {PouchType8SWSH::Treasures, "Treasures", 3800, 100},
            {PouchType8SWSH::Ingredients, "Ingredients", 4200, 100},
            {PouchType8SWSH::KeyItems, "Key Items", 4600, 64}};
        return pouches[static_cast<int>(type)];
    }
}

#endif