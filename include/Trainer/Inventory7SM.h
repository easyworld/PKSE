/**
 * GEN 7 DOES NOT SHARE GEN 6'S CODEC, which is the trap: the bag *shape* and the pouch table look
 * interchangeable. A Gen 7 slot is ONE PACKED u32 (PKHeX InventoryItem7):
 *
 *     bits  0-9   item id      (10 bits -- ids above 1023 cannot be stored)
 *     bits 10-19  count
 *     bits 20-29  free-space sort index
 *     bit  30     "new" flag
 *
 * Reading it as two u16s gives a plausible id and a count with the free-space index folded into
 * its top bits, and writing it back drops the sort order the player set. The free-space index and
 * the new flag are the player's, not ours: a writer that rebuilds a slot from id + count alone
 * silently reorders their bag, so Trainer7SM keeps the original word per slot and carries those
 * bits across when the id is unchanged.
 *
 * These pouches are NOT in offset order and their sizes are explicit rather than implied by the
 * next pouch, so both are listed. Offsets are relative to the MyItem block.
 */
#ifndef TRAINER_INVENTORY7_SM_H
#define TRAINER_INVENTORY7_SM_H

#include <cstddef>
#include <cstdint>

namespace Trainer
{
    /// One pouch's home in the bag: where it starts, how many 4-byte slots it holds, and the
    /// per-slot cap the game enforces.
    struct PouchLayout7SM
    {
        const char *name;
        size_t offset; // from the MyItem block
        size_t slots;
        int maxCount;
    };

    inline constexpr int POUCH_COUNT7_SM = 6;

    inline constexpr PouchLayout7SM POUCHES7_SM[POUCH_COUNT7_SM] = {
        {"Items", 0x000, 430, 999},
        {"Medicine", 0xb48, 64, 999},
        {"TMs/HMs", 0x998, 108, 1},
        {"Berries", 0xc48, 72, 999},
        {"Key Items", 0x6b8, 184, 1},
        {"Z-Crystals", 0xd68, 30, 1},
    };


    inline constexpr uint32_t ITEM_ID_MASK7_SM = 0x3FF;
    inline constexpr uint32_t ITEM_NEW_BIT7_SM = 0x40000000u;
    inline constexpr int ITEM_COUNT_SHIFT7_SM = 10;
    inline constexpr int ITEM_FREE_SHIFT7_SM = 20;
    /// Everything that is NOT id or count -- the free-space sort index and the new flag.
    inline constexpr uint32_t ITEM_CARRY_MASK7_SM = 0xFFF00000u;
}

#endif
