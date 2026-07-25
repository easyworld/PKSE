/**
 * Experience.h - EXP -> Level lookup and per-species growth rates
 *
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

namespace Pokemon {

    /**
     * Gets the current level for an amount of experience under a growth rate.
     *
     * Returns the highest level L in [1,100] whose total-EXP threshold is <= exp,
     * clamped to [1,100]. Mirrors PKHeX Experience.GetLevel.
     *
     * @param exp        Total experience points.
     * @param growthRate Growth-rate index 0-5 (see ordering above). Out-of-range
     *                   values fall back to Medium Fast (0).
     * @return Level in the range [1,100].
     */
    uint8_t getLevelFromExp(uint32_t exp, uint8_t growthRate) noexcept;

    /**
     * Gets the minimum total EXP required to reach a level under a growth rate.
     *
     * Inverse of getLevelFromExp: indexes the SAME six growth-rate total-EXP tables
     * and returns the threshold for `level`. Level is clamped to [1,100]. Level 1
     * returns the table's level-1 threshold (0 for every growth rate).
     *
     * @param level      Target level (clamped to [1,100]).
     * @param growthRate Growth-rate index 0-5 (see ordering above). Out-of-range
     *                   values fall back to Medium Fast (0).
     * @return Minimum total experience points for `level`.
     */
    uint32_t getExpForLevel(uint8_t level, uint8_t growthRate) noexcept;

    /**
     * Gets the EXP growth rate (0-5) for a national-dex species.
     *
     * Data extracted from PKHeX's Scarlet/Violet personal table (full national dex).
     *
     * @param species National dex number (1-1025).
     * @return Growth-rate index 0-5; 0 for species 0 or out-of-range.
     */
    uint8_t getGrowthRate(uint16_t species) noexcept;

}

#endif // POKEMON_EXPERIENCE_H
