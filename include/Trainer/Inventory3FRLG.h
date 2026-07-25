#ifndef TRAINER_INVENTORY3_FRLG_H
#define TRAINER_INVENTORY3_FRLG_H

#include <cstddef>

#include "Trainer/Inventory.h"

namespace Trainer {
    /**
     * FireRed/LeafGreen bag pouches. Offsets are relative to the Large block (sections 1-4) start.
     * Each slot is 4 bytes: u16 item id + u16 count. The bag pouch counts are XOR-obfuscated with the
     * save's security key (low 16 bits, Small+0xF20); the PC Items pouch is stored plaintext (key = 0).
     * Slot counts + offsets verified against PKHeX SAV3FRLG + the real save. See docs/ROADMAP_TO_V1.md App. A.
     */
    // Tab order == the in-game bag: the three real bag pockets (Items, Key Items, Poké Balls) come
    // first, then the inventories the game reaches via key items -- the TM Case, the Berry Pouch --
    // and finally the item PC. Those last three are not bag tabs in-game, but PKHeX exposes them and
    // they stay editable here (their names match what the game calls each container).
    enum class PouchType3FRLG {
        Items = 0,   // "Items"        -- general items (bag pocket)
        KeyItems,    // "Key Items"    -- key items (bag pocket)
        Balls,       // "Poké Balls"   -- Poké Balls (bag pocket)
        TMHM,        // "TM Case"      -- TMs & HMs (opened from the TM Case key item)
        Berries,     // "Berry Pouch"  -- berries (opened from the Berry Pouch key item)
        PCItems,     // "PC Items"     -- item PC storage (NOT key-obfuscated)
        Count
    };

    constexpr size_t POUCH_COUNT3_FRLG = static_cast<size_t>(PouchType3FRLG::Count);

    struct PouchInfo3FRLG {
        PouchType3FRLG type;
        const char* name;
        int offset;     // byte offset within the Large block
        int maxSlots;   // number of 4-byte slots
        bool keyed;     // true => count is XOR'd with the security key (low 16 bits)
    };

    inline const PouchInfo3FRLG& getPouchInfo3FRLG(PouchType3FRLG type) {
        static const PouchInfo3FRLG pouches[] = {
            {PouchType3FRLG::Items,    "道具",       0x310, 42, true},
            {PouchType3FRLG::KeyItems, "重要道具",   0x3B8, 30, true},
            {PouchType3FRLG::Balls,    "精灵球",  0x430, 13, true},
            {PouchType3FRLG::TMHM,     "招式学习器盒", 0x464, 58, true},
            {PouchType3FRLG::Berries,  "树果袋",       0x54C, 43, true},
            {PouchType3FRLG::PCItems,  "电脑中的道具", 0x298, 30, false},
        };
        int idx = static_cast<int>(type);
        if (idx < 0 || idx >= static_cast<int>(POUCH_COUNT3_FRLG)) idx = 0;
        return pouches[idx];
    }
}

#endif  // TRAINER_INVENTORY3_FRLG_H
