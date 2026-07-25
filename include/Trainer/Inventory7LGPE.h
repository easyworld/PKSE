#ifndef TRAINER_INVENTORY7_LGPE_H
#define TRAINER_INVENTORY7_LGPE_H

#include <cstdint>

#include "Trainer/Inventory.h"

namespace Trainer {
    /**
     * Let's Go item storage format (PKHeX InventoryItem7b), 4 bytes per slot:
     *   bits 0-14  : item ID   (read/written clamped to 0x7FF)
     *   bits 15-29 : count     (read/written clamped to 0x3FF, i.e. max 1023; game max is 999)
     *   bit  30    : "NEW" flag (bag highlight)
     *   bit  31    : reserved (unused)
     * NOTE: the field boundary is at bit 15 (NOT bit 10) — the count lives in the high half.
     */
    struct InventoryItem7LGPE : InventoryItem {
        // Decode from 32-bit value (matches PKHeX InventoryItem7b.GetValue)
        static InventoryItem7LGPE fromValue(uint32_t value) {
            InventoryItem7LGPE item;
            item.itemId = static_cast<uint16_t>(value & 0x7FF);          // item ID (bits 0-14, clamp 0x7FF)
            item.count  = static_cast<uint16_t>((value >> 15) & 0x3FF);  // count (bits 15-29, clamp 0x3FF)
            item.isNew  = (value & 0x40000000) != 0;                     // bit 30 = NEW
            item.isFavorite = false;                                     // bit 31 is reserved in Gen 7b
            return item;
        }

        // Encode to 32-bit value (matches PKHeX InventoryItem7b.GetValue; bit 31 left reserved)
        uint32_t toValue() const {
            uint32_t val = (static_cast<uint32_t>(itemId) & 0x7FF)
                         | ((static_cast<uint32_t>(count) & 0x3FF) << 15);
            if (isNew) val |= 0x40000000;
            return val;
        }
    };

    /**
     * Let's Go item pouches, ordered to match the in-game bag tabs (PKHeX PlayerBag7b).
     *
     * PKHeX exposes exactly SEVEN editable pouches for Let's Go -- there is NO editable
     * "Clothing" pouch. In-game the bag shows a Clothing tab, but outfits are a separate
     * wardrobe/customization system that is not stored as bag items (ItemStorage7GG has no
     * clothing span), so nothing here can edit it. The "Clothing Trunk" the player carries is
     * just a key item that rides in the General pool, which we surface as "Other Items".
     *
     * The last pouch ("Other Items", PKHeX's Items @ 0x0B38) is General + Key items MIXED --
     * Let's Go has no separate key-items pocket; the pocket key items (TM Case, Candy Jar, ...)
     * each ride there at a count of 1.
     *
     * Enum order == display order. Each pouch keeps its own PKHeX offset, so the enum can be
     * reordered freely as long as getPouchInfo7LGPE() below returns the matching offset.
     */
    enum class PouchType7LGPE {
        Medicine = 0,   // "Medicines"    -- Potions, etc.
        TMs,            // "TMs"          -- Technical Machines (max 1 each)
        PowerUp,        // "Power-Ups"    -- evolution stones, PP items
        Candy,          // "Candies"      -- stat / species candies
        Balls,          // "Poké Balls"
        Battle,         // "Battle Items" -- battle items + Mega Stones
        KeyItems,       // "Other Items"  -- PKHeX Items pouch: general + key items mixed
        Count           // Total number of pouches
    };

    // Number of pouches for Let's Go
    constexpr size_t POUCH_COUNT7_LGPE = static_cast<size_t>(PouchType7LGPE::Count);

    /**
     * Pouch information structure
     */
    struct PouchInfo7LGPE {
        PouchType7LGPE type;
        const char* name;
        int offset;     // Offset within MY_ITEM block
        int maxSlots;   // Maximum number of item slots in this pouch
    };

    /**
     * Get pouch info for a given type
     * Offsets based on PKHeX MyItem7b structure
     */
    inline const PouchInfo7LGPE& getPouchInfo7LGPE(PouchType7LGPE type) {
        // Offsets verified from PKHeX MyItem7b.cs
        // Each item is 4 bytes, slot count calculated from offset differences
        static const PouchInfo7LGPE pouches[] = {
            {PouchType7LGPE::Medicine, "回复道具",     0x0000, 60},   // 0x0000-0x00EF = 240 bytes = 60 slots
            {PouchType7LGPE::TMs,      "招式学习器",   0x00F0, 108},  // 0x00F0-0x029F = 432 bytes = 108 TMs
            {PouchType7LGPE::PowerUp,  "强化道具",     0x05C0, 150},  // 0x05C0-0x0817 = 600 bytes = 150 slots
            {PouchType7LGPE::Candy,    "糖果",         0x02A0, 200},  // 0x02A0-0x05BF = 800 bytes = 200 candies
            {PouchType7LGPE::Balls,    "精灵球",       0x0818, 50},   // 0x0818-0x08DF = 200 bytes = 50 slots
            {PouchType7LGPE::Battle,   "对战道具",     0x08E0, 150},  // 0x08E0-0x0B37 = 600 bytes = 150 slots
            {PouchType7LGPE::KeyItems, "其他道具",     0x0B38, 150}   // 0x0B38-0x0D8F: PKHeX "Items" pouch = general + key items mixed
        };
        return pouches[static_cast<int>(type)];
    }
}

#endif
