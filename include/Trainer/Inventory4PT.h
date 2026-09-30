/**
 * Uniform and simple compared with the GB generations: every pouch is a fixed run of 4-byte
 * entries, `u16 id` then `u16 count`. It is POSITIONAL, not packed -- a used-up item can sit past
 * a run of empty slots and keeps the free-space order the player gave it -- so a reader that stops
 * at the first zero id loses everything after it, and a writer that re-packs moves the rest.
 * Neither is something a checksum would catch. (PKHeX InventoryPouch4.)
 *
 * Offsets are RELATIVE TO THE BAG BASE, which is itself an offset into the General block and
 * belongs to the save layout rather than the bag -- see Trainer4PT::BAG_BASE.
 */
#ifndef TRAINER_INVENTORY4_PT_H
#define TRAINER_INVENTORY4_PT_H

#include <cstddef>
#include <cstdint>

namespace Trainer
{
    /// One pouch's home in the bag: where it starts, how many 4-byte slots it holds, and the
    /// per-slot cap the game enforces.
    struct PouchLayout4PT
    {
        const char *name;
        size_t offset; // from the bag base
        size_t slots;
        int maxCount;
    };

    inline constexpr int POUCH_COUNT4_PT = 8;

    inline constexpr PouchLayout4PT POUCHES4_PT[POUCH_COUNT4_PT] = {
        {"Items", 0x000, 165, 999},
        {"Key Items", 0x294, 50, 1},
        {"TMs/HMs", 0x35c, 100, 99},
        {"Mail", 0x4ec, 12, 999},
        {"Medicine", 0x51c, 40, 999},
        {"Berries", 0x5bc, 64, 999},
        {"Poke Balls", 0x6bc, 15, 999},
        {"Battle Items", 0x6f8, 30, 999},
    };

}

#endif
