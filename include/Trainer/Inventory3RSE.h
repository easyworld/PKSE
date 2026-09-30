#ifndef TRAINER_INVENTORY3_RSE_H
#define TRAINER_INVENTORY3_RSE_H

#include <cstddef>

#include "Trainer/Inventory.h"

namespace Trainer
{
    /**
     * Ruby/Sapphire/Emerald bag pouches. Offsets are relative to the Large block (sections 1-4)
     * start, the same convention Inventory3FRLG uses. Each slot is 4 bytes: u16 item id + u16 count.
     *
     * RUBY/SAPPHIRE AND EMERALD DIFFER, which is the whole reason this table takes a layout:
     *
     *  - The bag is 0x360 bytes in R/S and 0x3B0 in Emerald, and every pouch after PC Items moves.
     *    Emerald's Items and Key Items pouches are 30 slots where R/S have 20.
     *  - R/S DO NOT OBFUSCATE ANYTHING. Their save has no security key at all (PKHeX's
     *    SaveBlock3SmallRS.SecurityKey is a literal 0), so every count is plaintext. Emerald keys
     *    the bag exactly as FireRed/LeafGreen do -- and, like them, leaves the PC Items pouch alone.
     *
     * Getting the second one wrong is invisible rather than loud: XOR-ing a plaintext count with a
     * key of 0 is a no-op, so R/S works either way, while reading Emerald's bag without the key
     * gives item counts in the tens of thousands. The key is at Small+0x0AC in Emerald, NOT at
     * Small+0xF20 where FireRed/LeafGreen keep theirs.
     *
     * Offsets are PKHeX's PlayerBag3RS / PlayerBag3E, converted from inventory-relative to
     * Large-relative by adding the bag's own offset (0x498). Both tables end exactly on the block
     * length PKHeX declares -- R/S at 0x7F8 (0x498 + 0x360), Emerald at 0x848 (0x498 + 0x3B0) --
     * which is the arithmetic check that they were transcribed correctly.
     */

    /// Which of the trio a save is. Ruby and Sapphire are one layout; Emerald is its own.
    enum class Gen3HoennLayout
    {
        RubySapphire,
        Emerald
    };

    // Tab order == the in-game bag: the real pockets first, then the containers reached through a
    // key item, then the item PC. Same ordering as Inventory3FRLG, so the two read alike.
    enum class PouchType3RSE
    {
        Items = 0, // "Items"        -- general items (bag pocket)
        KeyItems,  // "Key Items"    -- key items (bag pocket)
        Balls,     // "Poké Balls"   -- Poké Balls (bag pocket)
        TMHM,      // "TMs & HMs"    -- the TM case (a bag pocket in Hoenn, unlike FR/LG)
        Berries,   // "Berries"      -- the berry pocket
        PCItems,   // "PC Items"     -- item PC storage (NEVER key-obfuscated)
        Count
    };

    constexpr size_t POUCH_COUNT3_RSE = static_cast<size_t>(PouchType3RSE::Count);

    struct PouchInfo3RSE
    {
        PouchType3RSE type;
        const char *name;
        int offset;   // byte offset within the Large block
        int maxSlots; // number of 4-byte slots
        bool keyed;   // true => count is XOR'd with the security key (low 16 bits)
    };

    inline const PouchInfo3RSE &getPouchInfo3RSE(PouchType3RSE type, Gen3HoennLayout layout)
    {
        // Ruby/Sapphire: bag 0x498..0x7F8. Nothing is keyed -- there is no key.
        static const PouchInfo3RSE rubySapphire[] = {
            {PouchType3RSE::Items, "Items", 0x560, 20, false},
            {PouchType3RSE::KeyItems, "Key Items", 0x5B0, 20, false},
            {PouchType3RSE::Balls, "Poké Balls", 0x600, 16, false},
            {PouchType3RSE::TMHM, "TMs & HMs", 0x640, 64, false},
            {PouchType3RSE::Berries, "Berries", 0x740, 46, false},
            {PouchType3RSE::PCItems, "PC Items", 0x498, 50, false},
        };
        // Emerald: bag 0x498..0x848. Keyed like FR/LG, PC Items excepted.
        static const PouchInfo3RSE emerald[] = {
            {PouchType3RSE::Items, "Items", 0x560, 30, true},
            {PouchType3RSE::KeyItems, "Key Items", 0x5D8, 30, true},
            {PouchType3RSE::Balls, "Poké Balls", 0x650, 16, true},
            {PouchType3RSE::TMHM, "TMs & HMs", 0x690, 64, true},
            {PouchType3RSE::Berries, "Berries", 0x790, 46, true},
            {PouchType3RSE::PCItems, "PC Items", 0x498, 50, false},
        };
        int index = static_cast<int>(type);
        if (index < 0 || index >= static_cast<int>(POUCH_COUNT3_RSE))
            index = 0;
        return layout == Gen3HoennLayout::Emerald ? emerald[index] : rubySapphire[index];
    }
}

#endif // TRAINER_INVENTORY3_RSE_H
