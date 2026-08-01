/**
 * SpeciesConverter9.h - Gen 9 internal <-> National Dex species index conversion.
 *
 * Scarlet/Violet and Legends: Z-A do NOT store the National Dex number in an entity's
 * species field (u16 @ 0x08). From #917 (Tarountula) onward the games' INTERNAL species
 * order diverges from the final National Dex assignment, so the stored value must be
 * converted on every read and write -- exactly like Gen 3 (see g3ToNational /
 * nationalToG3 in Pokemon3FRLG.h, the same concept for FR/LG's internal order).
 *
 * PKHeX reference (source of truth):
 *   - PK9.cs / PA9.cs: `Species => SpeciesConverter.GetNational9(SpeciesInternal)`
 *   - PKHeX.Core/PKM/Util/Conversion/SpeciesConverter.cs:
 *       FirstUnalignedNational9 = 917; Table9NationalToInternal / Table9InternalToNational
 *
 * Reading the raw value as a dex number displayed the WRONG species (wrong name, sprite,
 * types, base stats, learnset -> false legality errors) for every mon whose stored id is
 * >= 917. Both tables are delta tables: result = input + table[input - 917]; identity for
 * anything outside the table (species < 917, and future ids past the table's end).
 * Do not hand-edit the deltas -- they are copied verbatim from PKHeX.
 */

#ifndef POKEMON_SPECIES_CONVERTER9_H
#define POKEMON_SPECIES_CONVERTER9_H

#include <cstdint>

namespace Pokemon {

    /// First species id where the Gen 9 internal order and the National Dex diverge.
    inline constexpr uint16_t GEN9_FIRST_UNALIGNED_SPECIES = 917;

    /// Deltas indexed by (national - 917): internal = national + delta. PKHeX Table9NationalToInternal.
    inline constexpr int8_t GEN9_NATIONAL_TO_INTERNAL[109] = {
                                  1,   1,   1,
          1,  33,  33,  33,  21,  21,  44,  44,   7,   7,
          7,  29,  31,  31,  31,  68,  68,  68,   2,   2,
         17,  17,  30,  30,  24,  24,  28,  28,  58,  58,
         12, -13, -13, -31, -31, -29, -29,  43,  43,  43,
        -31, -31,  -3, -30, -30, -23, -23, -14, -24,  -3,
         -3, -47, -47, -12, -27, -27, -44, -46, -26,  31,
         29, -53, -65,  25,  -6,  -3,  -7,  -4,  -4,  -8,
         -4,   1,  -3,  -3,  -6,  -4, -47, -47, -47, -23,
        -23,  -5,  -7,  -9,  -7, -20, -13,  -9,  -9, -29,
        -23,   1,  12,  12,   0,   0,   0,  -6,   5,  -6,
         -3,  -3,  -2,  -4,  -3,  -3,
    };

    /// Deltas indexed by (internal - 917): national = internal + delta. PKHeX Table9InternalToNational.
    inline constexpr int8_t GEN9_INTERNAL_TO_NATIONAL[109] = {
                                 65,  -1,  -1,
         -1,  -1,  31,  31,  47,  47,  29,  29,  53,  31,
         31,  46,  44,  30,  30,  -7,  -7,  -7,  13,  13,
         -2,  -2,  23,  23,  24, -21, -21,  27,  27,  47,
         47,  47,  26,  14, -33, -33, -33, -17, -17,   3,
        -29,  12, -12, -31, -31, -31,   3,   3, -24, -24,
        -44, -44, -30, -30, -28, -28,  23,  23,   6,   7,
         29,   8,   3,   4,   4,  20,   4,  23,   6,   3,
          3,   4,  -1,  13,   9,   7,   5,   7,   9,   9,
        -43, -43, -43, -68, -68, -68, -58, -58, -25, -29,
        -31,   6,  -1,   6,   0,   0,   0,   3,   3,   4,
          2,   3,   3,  -5, -12, -12,
    };

    /// Raw stored species (SV / Z-A internal index) -> National Dex number. Identity outside the table.
    inline constexpr uint16_t gen9InternalToNational(uint16_t raw) noexcept {
        const uint32_t shift = static_cast<uint32_t>(raw) - GEN9_FIRST_UNALIGNED_SPECIES;
        if (shift >= sizeof(GEN9_INTERNAL_TO_NATIONAL)) return raw;   // < 917 wraps huge -> identity
        return static_cast<uint16_t>(raw + GEN9_INTERNAL_TO_NATIONAL[shift]);
    }

    /// National Dex number -> raw stored species (SV / Z-A internal index). Identity outside the table.
    inline constexpr uint16_t gen9NationalToInternal(uint16_t national) noexcept {
        const uint32_t shift = static_cast<uint32_t>(national) - GEN9_FIRST_UNALIGNED_SPECIES;
        if (shift >= sizeof(GEN9_NATIONAL_TO_INTERNAL)) return national;
        return static_cast<uint16_t>(national + GEN9_NATIONAL_TO_INTERNAL[shift]);
    }

    // Compile-time spot checks from the field report that exposed this (all PKHeX-verified):
    // stored 967 must read Glimmora (970), 1003 -> Charcadet (935), 917 -> Dudunsparce (982).
    static_assert(gen9InternalToNational(967) == 970,  "Gen9 species table: 967 -> Glimmora");
    static_assert(gen9InternalToNational(1003) == 935, "Gen9 species table: 1003 -> Charcadet");
    static_assert(gen9InternalToNational(917) == 982,  "Gen9 species table: 917 -> Dudunsparce");
    static_assert(gen9NationalToInternal(970) == 967,  "Gen9 species table: Glimmora -> 967");
    static_assert(gen9NationalToInternal(935) == 1003, "Gen9 species table: Charcadet -> 1003");
    static_assert(gen9NationalToInternal(916) == 916,  "Gen9 species table: aligned below 917");
    static_assert(gen9InternalToNational(0) == 0,      "Gen9 species table: empty slot is identity");
}

#endif
