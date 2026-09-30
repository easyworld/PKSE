/**
 * SIX SAVE-FORMAT GROUPS DESCRIBE THEIR SAVE AS A FIXED LIST OF BLOCKS, and each has its own
 * table: Blocks5BW, Blocks5B2W2, Blocks6XY, Blocks6ORAS, Blocks7SM and Blocks7USUM. Nothing else
 * does, and nothing else should:
 *
 *   * Gens 1 and 2 are flat SRAM whose offsets shift with the save's LOCALE, Gen 3 is fourteen
 *     rotated sectors that each carry their own id, Gen 4 is two partitions holding a General and
 *     a Storage block, and BDSP is a flat fixed-offset image. Those are a handful of constants or
 *     plain arithmetic, and a block table there would be inventing structure that is not there.
 *   * Let's Go, Sword/Shield, Legends: Arceus, Scarlet/Violet and Legends: Z-A use SCBlocks --
 *     key, type and data, discovered by walking the save. A static table for those is not merely
 *     unnecessary, it is IMPOSSIBLE: the block list is data inside the file and differs per save.
 *
 * THE ROW TYPE IS PER PLATFORM, NOT PER GAME, because that is what actually varies. A DS save
 * stores each block's CRC twice and a 3DS save stores it in a metadata chunk at the end of the
 * file; every game on a platform does it the same way. The TABLES are per group, one file each,
 * because the offsets and lengths are what differ -- and a table shared between two games is how
 * a Black/White save gets read with Black 2's geometry.
 */
#ifndef TRAINER_BLOCKTABLE_H
#define TRAINER_BLOCKTABLE_H

#include <cstddef>
#include <cstdint>

namespace Trainer
{
    /**
     * One block of a DS-era (Gen 5) save.
     *
     * The CRC16-CCITT over [offset, offset + length) is stored TWICE and BOTH copies must be
     * written or the game rejects the save: once just past the data, and once in the trailing
     * checksum block. That last block holds every other block's mirror and checksums ITSELF, so
     * it has to be stamped last -- stamped any earlier and it covers stale mirror bytes.
     */
    struct BlockEntryNDS
    {
        uint32_t offset;
        uint32_t length;
        uint32_t checksumOffset;   ///< CRC slot immediately past this block's data
        uint32_t checksumMirror;   ///< the same CRC again, inside the checksum block
    };

    /**
     * One block of a 3DS-era (Gen 6 / Gen 7) `main` save.
     *
     * There is no per-block checksum slot here. Every checksum lives in the BEEF metadata chunk
     * in the file's last 0x200 bytes -- see blockChecksumOffset3DS.
     */
    struct BlockEntry3DS
    {
        uint32_t offset;
        uint32_t length;
    };

    /**
     * Where block `blockId`'s checksum sits inside a 3DS save's BEEF chunk.
     *
     *     @ fileSize - 0x200:  u64 timestamp1, u64 timestamp2, "BEEF",
     *                          then { u32 length, u16 id, u16 checksum } per block
     *
     * Gen 6 checksums with CRC16-CCITT and Gen 7 with CRC16Invert -- same width, different
     * algorithm, and nothing in the data says which one produced a given value. Each container
     * picks its own; there is no shared helper for it on purpose.
     */
    inline constexpr size_t blockChecksumOffset3DS(size_t fileSize, size_t blockId) noexcept
    {
        return (fileSize - 0x200) + 0x14 + blockId * 8 + 6;
    }
}

#endif  // TRAINER_BLOCKTABLE_H
