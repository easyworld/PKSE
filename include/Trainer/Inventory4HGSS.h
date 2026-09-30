/**
 * Uniform and simple compared with the GB generations: every pouch is a fixed run of 4-byte
 * entries, `u16 id` then `u16 count`. It is POSITIONAL, not packed -- a used-up item can sit past
 * a run of empty slots and keeps the free-space order the player gave it -- so a reader that stops
 * at the first zero id loses everything after it, and a writer that re-packs moves the rest.
 * Neither is something a checksum would catch. (PKHeX InventoryPouch4.)
 *
 * Offsets are RELATIVE TO THE BAG BASE, which is itself an offset into the General block and
 * belongs to the save layout rather than the bag -- see Trainer4HGSS::BAG_BASE.
 */
#ifndef TRAINER_INVENTORY4_HGSS_H
#define TRAINER_INVENTORY4_HGSS_H

#include <cstddef>
#include <cstdint>

namespace Trainer
{
    /// One pouch's home in the bag: where it starts, how many 4-byte slots it holds, and the
    /// per-slot cap the game enforces.
    struct PouchLayout4HGSS
    {
        const char *name;
        size_t offset; // from the bag base
        size_t slots;
        int maxCount;
    };

    inline constexpr int POUCH_COUNT4_HGSS = 8;

    inline constexpr PouchLayout4HGSS POUCHES4_HGSS[POUCH_COUNT4_HGSS] = {
        {"Items", 0x000, 165, 999},
        {"Key Items", 0x294, 50, 1},
        {"TMs/HMs", 0x35c, 100, 99},
        {"Mail", 0x4f0, 12, 999},
        {"Medicine", 0x520, 40, 999},
        {"Berries", 0x5c0, 64, 999},
        {"Poke Balls", 0x6c0, 24, 999},
        {"Battle Items", 0x720, 30, 999},
    };

}

#endif
