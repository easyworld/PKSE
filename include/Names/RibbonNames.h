/**
 * RibbonNames.h - Ribbon & Mark lookup (per generation)
 *
 * Reads the set ribbon/mark bit-flags out of a Pokemon's DECRYPTED entity
 * buffer and returns clean display names for every flag that is set.
 *
 * Ribbons and marks are stored as individual BIT FLAGS at fixed byte offsets in
 * the decrypted PKM data (the same buffer PKHeX indexes as Data[...]). The byte
 * offsets and the ribbon/mark set differ by generation:
 *
 *   - Gen 8/9 HOME format: 0x34..0x45 block (58 ribbons + 53 marks = 111 flags).
 *     PKHeX's G8PKM (PK8/PB8 = Sword/Shield + Brilliant Diamond/Shining Pearl),
 *     PA8 (Legends: Arceus) and PK9 (Scarlet/Violet + Legends: Z-A) all use the
 *     IDENTICAL layout here (verified byte-for-byte against the PKHeX source).
 *   - Gen 7 Let's Go (PB7 = GG): 0x30..0x36 block (50 ribbons, no marks).
 *   - Gen 3 FireRed/LeafGreen (PK3 = FRLG): a single u32 at 0x4C. The odd one
 *     out -- the five contest ribbons are 3-bit LEVELS (bits 0-14), not flags,
 *     so only the highest rank reached is reported. Bits 15-26 are flags,
 *     27-30 unused, and bit 31 is FatefulEncounter rather than a ribbon.
 *
 * Offsets and names are transcribed directly from PKHeX
 * (PKHeX.Core/PKM/Shared/G8PKM.cs, PKHeX.Core/PKM/PK9.cs, PA8.cs, PB7.cs).
 * The byte-VALUE ribbon fields (RibbonCountMemoryContest / RibbonCountMemoryBattle
 * / AffixedRibbon) are not bit flags and are intentionally omitted, as are the
 * unused placeholder bits (RIB45_7, RIB46_x, RIB47_x, RIB6_2..7).
 */

#ifndef NAMES_RIBBON_NAMES_H
#define NAMES_RIBBON_NAMES_H

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

#include "Enums/GameVersion.h"

namespace Names
{
    /**
     * A ribbon name in one specific language, or nullptr when this table has none for it.
     *
     * Generated from PKHeX (tools/gen_ribbonnames.py) and carries the EIGHT NON-ENGLISH languages
     * only. PKSE's English names are the ones the GAMES show, which is not always PKHeX's UI label
     * -- "Champion" rather than "Champion (Gen3)", "Cool Ribbon Super" rather than "Cool Super" --
     * so English is never taken from here. Looked up BY the English string, which is why every name
     * RibbonNames.cpp can produce is asserted to have a row.
     */
    const char *getRibbonNameLocalized(const char *englishName, size_t languageIndex);
    /**
     * Returns the display names of every ribbon/mark set on a pokemon.
     *
     * @param entityData Pointer to the pokemon's DECRYPTED entity buffer, indexed
     *                   from offset 0 (entityData[0x34] is PKHeX's Data[0x34]).
     *                   Must be non-null and large enough to cover the group's
     *                   ribbon block: >= 0x46 bytes for Gen 8/9, >= 0x37 for
     *                   Gen 7 Let's Go, >= 0x50 for Gen 3 FireRed/LeafGreen.
     * @param group      The pokemon's game group (Enums::GameVersion). Accepts the
     *                   group ids (GG, SWSH, BDSP, PLA, SV, ZA) as well as the
     *                   individual game ids (GP/GE, SW/SH, BD/SP, PLA, SL/VL, ZA).
     * @return Names of every SET ribbon/mark, in a stable (byte, bit) order.
     *         Empty vector when none are set, the buffer is null, or the group
     *         is not handled.
     */
    std::vector<std::string> getMonRibbons(const uint8_t *entityData, Enums::GameVersion group);
}

#endif
