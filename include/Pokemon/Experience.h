/**
 * Mirrors PKHeX's Experience math (PKHeX.Core/PKM/Util/Experience.cs). Provides the
 * six growth-rate total-EXP tables (levels 1-100) via getLevelFromExp(), plus a
 * national-dex growth-rate lookup extracted from PKHeX's Scarlet/Violet personal table.
 *
 * Primary use: deriving a Pokemon's level from its stored EXP when the cached
 * party-stat level byte is unavailable (e.g. Legends: Arceus box slots are stored-size
 * and carry no party-stat block, so data[0x168] reads as 0). This is game-agnostic.
 *
 * Growth-rate index ordering (matches PKHeX Experience.cs GetTable() / the GrowthRate enum):
 *   0 = Medium Fast   (level-100 total = 1,000,000)
 *   1 = Erratic       (level-100 total =   600,000)
 *   2 = Fluctuating   (level-100 total = 1,640,000)
 *   3 = Medium Slow   (level-100 total = 1,059,860)
 *   4 = Fast          (level-100 total =   800,000)
 *   5 = Slow          (level-100 total = 1,250,000)
 */

#ifndef POKEMON_EXPERIENCE_H
#define POKEMON_EXPERIENCE_H

#include <cstdint>

namespace Pokemon
{

    /// The highest level in [1,100] whose total-EXP threshold is <= exp. An out-of-range growth
    /// rate falls back to Medium Fast.
    uint8_t getLevelFromExp(uint32_t exp, uint8_t growthRate) noexcept;

    /// Inverse of getLevelFromExp, off the same six tables. An out-of-range growth rate falls
    /// back to Medium Fast.
    uint32_t getExpForLevel(uint8_t level, uint8_t growthRate) noexcept;

    /// Read from the Scarlet/Violet personal table, the only one spanning the whole National Dex.
    uint8_t getGrowthRate(uint16_t species) noexcept;

}

#endif // POKEMON_EXPERIENCE_H
