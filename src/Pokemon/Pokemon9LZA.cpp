/**
 * Pokemon9LZA.cpp - Generation 9 Pokemon Data Class Implementation
 *
 * Implementation of Pokemon9LZA class methods including stat calculations,
 * gender determination, shiny manipulation, and PID regeneration.
 */

#include <cstdio>
#include <algorithm>

#include "Pokemon/Pokemon9LZA.h"
#include "Pokemon/BaseStatsGen89.h"
#include "Pokemon/Experience.h"

namespace Pokemon {
    // Forward declarations for external helper functions
    // These are defined in other files and provide species/item/nature name lookups
    extern const char* getSpeciesNameGen89(uint16_t speciesId);

    // ========================================
    // Species and Name Lookups
    // ========================================

    const char* Pokemon9LZA::species() const noexcept
    {
        return getSpeciesNameGen89(speciesID());
    }

    // ========================================
    // Gender Determination
    // ========================================

    uint8_t Pokemon9LZA::gender() const noexcept
    {
        // Gen 9 stores gender explicitly (0 = Male, 1 = Female, 2 = Genderless) in
        // bits 1-2 of byte 0x22 (note: PA9 uses a different bit offset than PK8's
        // bits 2-3). This is authoritative — genderless species are stored as 2 —
        // and independent of the PID, so it survives shiny/PID edits (unlike the old
        // PID-derived approximation).
        return (static_cast<uint8_t>(data[0x22]) >> 1) & 0x03;
    }

    // ========================================
    // Base Stats (Species-Dependent)
    // ========================================

    uint8_t Pokemon9LZA::baseHP() const noexcept
    {
        return getBaseStatsGen89(speciesID(), form())->hp;
    }

    uint8_t Pokemon9LZA::baseATK() const noexcept
    {
        return getBaseStatsGen89(speciesID(), form())->atk;
    }

    uint8_t Pokemon9LZA::baseDEF() const noexcept
    {
        return getBaseStatsGen89(speciesID(), form())->def;
    }

    uint8_t Pokemon9LZA::baseSPE() const noexcept
    {
        return getBaseStatsGen89(speciesID(), form())->spe;
    }

    uint8_t Pokemon9LZA::baseSPA() const noexcept
    {
        return getBaseStatsGen89(speciesID(), form())->spa;
    }

    uint8_t Pokemon9LZA::baseSPD() const noexcept
    {
        return getBaseStatsGen89(speciesID(), form())->spd;
    }

    // ========================================
    // Nature Modifier
    // ========================================

    int Pokemon9LZA::getNatureModifier(int statIndex) const noexcept
    {
        /**
         * Returns the nature modifier for a given stat.
         *
         * Nature affects 5 stats (all except HP):
         * - Increased stat: 110 (multiply by 110, divide by 100 = 1.1x)
         * - Decreased stat: 90 (multiply by 90, divide by 100 = 0.9x)
         * - Neutral: 100 (multiply by 100, divide by 100 = 1.0x)
         *
         * Gen 8 uses StatNature (affected by mints) instead of Nature for calculations.
         * Mints allow changing effective nature without changing the Pokemon's actual nature.
         *
         * Nature Table Format:
         * Each nature has 5 modifiers for [ATK, DEF, SPE, SPA, SPD]:
         * - 0 = decreased (0.9x)
         * - 1 = neutral (1.0x)
         * - 2 = increased (1.1x)
         */

        uint8_t nature = statNature();

        // Nature modifier table [25 natures][5 stats]
        // Stats: 0=ATK, 1=DEF, 2=SPE, 3=SPA, 4=SPD
        static const int8_t natureTable[25][5] = {
            {1, 1, 1, 1, 1}, // 0: Hardy (neutral)
            {2, 0, 1, 1, 1}, // 1: Lonely (+Atk, -Def)
            {2, 1, 0, 1, 1}, // 2: Brave (+Atk, -Spe)
            {2, 1, 1, 0, 1}, // 3: Adamant (+Atk, -SpA)
            {2, 1, 1, 1, 0}, // 4: Naughty (+Atk, -SpD)
            {0, 2, 1, 1, 1}, // 5: Bold (-Atk, +Def)
            {1, 1, 1, 1, 1}, // 6: Docile (neutral)
            {1, 2, 0, 1, 1}, // 7: Relaxed (+Def, -Spe)
            {1, 2, 1, 0, 1}, // 8: Impish (+Def, -SpA)
            {1, 2, 1, 1, 0}, // 9: Lax (+Def, -SpD)
            {0, 1, 2, 1, 1}, // 10: Timid (-Atk, +Spe)
            {1, 0, 2, 1, 1}, // 11: Hasty (-Def, +Spe)
            {1, 1, 1, 1, 1}, // 12: Serious (neutral)
            {1, 1, 2, 0, 1}, // 13: Jolly (+Spe, -SpA)
            {1, 1, 2, 1, 0}, // 14: Naive (+Spe, -SpD)
            {0, 1, 1, 2, 1}, // 15: Modest (-Atk, +SpA)
            {1, 0, 1, 2, 1}, // 16: Mild (-Def, +SpA)
            {1, 1, 0, 2, 1}, // 17: Quiet (+SpA, -Spe)
            {1, 1, 1, 1, 1}, // 18: Bashful (neutral)
            {1, 1, 1, 2, 0}, // 19: Rash (+SpA, -SpD)
            {0, 1, 1, 1, 2}, // 20: Calm (-Atk, +SpD)
            {1, 0, 1, 1, 2}, // 21: Gentle (-Def, +SpD)
            {1, 1, 0, 1, 2}, // 22: Sassy (+SpD, -Spe)
            {1, 1, 1, 0, 2}, // 23: Careful (+SpD, -SpA)
            {1, 1, 1, 1, 1}, // 24: Quirky (neutral)
        };

        // Validate inputs
        if (nature >= 25 || statIndex < 0 || statIndex >= 5) {
            return 100; // Neutral if invalid
        }

        int modifier = natureTable[nature][statIndex];
        return modifier == 0 ? 90 : (modifier == 2 ? 110 : 100);
    }

    // ========================================
    // Stat Recalculation
    // ========================================

    void Pokemon9LZA::recalculateStats() noexcept
    {
        /**
         * Recalculates all battle stats based on current IVs, EVs, base stats, level, and nature.
         *
         * Stat Formulas (Gen 3+):
         * HP = ((2 * Base + IV + EV/4) * Level / 100) + Level + 10
         * Other Stats = (((2 * Base + IV + EV/4) * Level / 100) + 5) * NatureMod / 100
         *
         * Where:
         * - Base = Base stat for the species
         * - IV = Individual Value (0-31)
         * - EV = Effort Value (0-252)
         * - Level = Pokemon's current level (1-100)
         * - NatureMod = Nature modifier (90, 100, or 110)
         */

        uint8_t levelValue = level();
        if (levelValue == 0 || levelValue > 100) {
            return; // Invalid level, don't recalculate
        }

        // Calculate HP (different formula from other stats)
        int hp = ((2 * baseHP() + ivHP() + (evHP() / 4)) * levelValue) / 100 + levelValue + 10;
        const uint16_t oldMax = readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x14A));   // before it is overwritten
        writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x14A), static_cast<uint16_t>(hp));
        if (oldMax != static_cast<uint16_t>(hp))
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x8A), static_cast<uint16_t>(hp));

        // Calculate other stats (ATK, DEF, SPE, SPA, SPD)
        int stats[5];
        int baseStats[5] = {baseATK(), baseDEF(), baseSPE(), baseSPA(), baseSPD()};
        int ivs[5] = {ivATK(), ivDEF(), ivSPE(), ivSPA(), ivSPD()};
        int evs[5] = {evATK(), evDEF(), evSPE(), evSPA(), evSPD()};

        for (int i = 0; i < 5; i++) {
            // Base stat calculation
            int baseStat = ((2 * baseStats[i] + ivs[i] + (evs[i] / 4)) * levelValue) / 100 + 5;

            // Apply nature modifier
            int modifier = getNatureModifier(i);
            stats[i] = (baseStat * modifier) / 100;
        }

        // Write calculated stats to party stat section
        writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x14C), static_cast<uint16_t>(stats[0])); // ATK
        writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x14E), static_cast<uint16_t>(stats[1])); // DEF
        writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x150), static_cast<uint16_t>(stats[2])); // SPE
        writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x152), static_cast<uint16_t>(stats[3])); // SPA
        writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x154), static_cast<uint16_t>(stats[4])); // SPD
    }

    // ========================================
    // Set Level
    // ========================================

    void Pokemon9LZA::setLevel(uint8_t level) noexcept
    {
        // Clamp to the valid level range [1,100].
        if (level < 1) level = 1;
        if (level > 100) level = 100;

        // EXP (0x10) is the source of truth for level: write this level's minimum total EXP.
        uint32_t exp = getExpForLevel(level, getGrowthRate(speciesID()));
        writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x10), exp);

        // Update the cached party-stat level byte that level() reads (0x148).
        data[0x148] = static_cast<std::byte>(level);

        recalculateStats();
        refreshChecksum();
    }

    void Pokemon9LZA::setExp(uint32_t value) noexcept
    {
        // Write raw total EXP (0x10), then re-derive and cache the level from it (0x148).
        writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x10), value);
        uint8_t level = getLevelFromExp(value, getGrowthRate(speciesID()));
        if (level < 1) level = 1;
        if (level > 100) level = 100;
        data[0x148] = static_cast<std::byte>(level);
        recalculateStats();
        refreshChecksum();
    }

    // ========================================
    // PID Regeneration
    // ========================================

    void Pokemon9LZA::regeneratePID(uint32_t trainerID32) noexcept
    {
        /**
         * Regenerates PID while maintaining gender and shininess.
         *
         * This is useful when IVs are modified
         * that may flag the Pokemon as invalid if PID doesn't match certain criteria.
         *
         * The algorithm uses an iterative approach to find a PID that satisfies
         * BOTH the shiny constraint AND the gender constraint simultaneously,
         * rather than adjusting them sequentially (which would break shininess).
         */

        // Save current properties
        bool wasShiny = isShiny(trainerID32, species());
        uint32_t currentPID = pid();
        uint8_t genderByte = currentPID & 0xFF;

        // Generate a base PID using EC as seed
        uint32_t ec = encryptionConstant();
        uint32_t basePID = ec ^ 0x13371337;

        if (wasShiny) {
            // Use iterative approach to find PID that is BOTH shiny AND preserves gender
            uint32_t baseHighWord = basePID >> 16;
            uint32_t tidHigh = trainerID32 >> 16;
            uint32_t tidLow = trainerID32 & 0xFFFF;

            // Try different high words (start with base, then try variations)
            for (int attempt = 0; attempt < 256; attempt++) {
                uint32_t highWord = (baseHighWord + attempt) & 0xFFFF;
                uint32_t H = highWord ^ tidHigh;

                // Try xorResult values 1-15 (star shiny)
                for (int targetXor = 1; targetXor < 16; targetXor++) {
                    uint32_t L = H ^ targetXor;
                    uint32_t lowWord = L ^ tidLow;

                    // Check if this preserves gender byte
                    if ((lowWord & 0xFF) == genderByte) {
                        // Found a match! Construct new PID
                        uint32_t newPID = (highWord << 16) | lowWord;
                        writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
                        refreshChecksum();
                        return;
                    }
                }

                // Also try targetXor = 0 (square shiny)
                uint32_t L = H ^ 0;
                uint32_t lowWord = L ^ tidLow;
                if ((lowWord & 0xFF) == genderByte) {
                    uint32_t newPID = (highWord << 16) | lowWord;
                    writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
                    refreshChecksum();
                    return;
                }
            }

            // Fallback: preserve shiny status even if gender byte can't be preserved exactly
            uint32_t highWord = baseHighWord & 0xFFFF;
            uint32_t H = highWord ^ tidHigh;
            uint32_t L = H ^ 1; // Star shiny
            uint32_t lowWord = L ^ tidLow;
            uint32_t newPID = (highWord << 16) | lowWord;
            writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
            refreshChecksum();

        } else {
            // Non-shiny: preserve gender byte first, then ensure non-shiny
            uint32_t newPID = (basePID & 0xFFFFFF00) | genderByte;

            // Check if still shiny, if so adjust high bits (not low byte)
            uint32_t xorValue = ((newPID >> 16) ^ (newPID & 0xFFFF) ^ (trainerID32 >> 16) ^ (trainerID32 & 0xFFFF)) & 0xFFFF;
            if (xorValue < 16) {
                // Flip bit 8 of low word to break shiny while preserving gender byte
                newPID ^= 0x100;
            }

            writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
            refreshChecksum();
        }
    }

    // ========================================
    // Shiny Manipulation
    // ========================================

    void Pokemon9LZA::setShiny(bool makeShiny, uint32_t trainerID32) noexcept
    {
        /**
         * Sets the Pokemon's shiny status while maintaining gender.
         *
         * Shiny Pokemon have alternate coloration and are determined by the XOR
         * of their PID and the trainer's ID:
         * XOR = (PID_High ^ PID_Low ^ TID16 ^ SID16)
         * - Shiny if XOR < 16
         * - Square shiny if XOR = 0
         * - Star shiny if XOR = 1-15
         *
         * This function modifies the PID to achieve the desired shiny status
         * while preserving the Pokemon's gender.
         */

        bool currentlyShiny = isShiny(trainerID32, species());

        // If already in desired state, do nothing
        if (currentlyShiny == makeShiny) {
            return;
        }

        // Get current PID and preserve gender byte
        uint32_t currentPID = pid();
        uint8_t genderByte = currentPID & 0xFF;

        if (makeShiny) {
            /**
             * Make Pokemon shiny while preserving gender byte.
             *
             * Algorithm:
             * 1. Try different high word values
             * 2. For each high word, try different XOR results (0-15, all shiny)
             * 3. Find a combination where the low byte matches the gender byte
             * 4. Use that PID
             */

            uint32_t ec = encryptionConstant();
            uint32_t baseHighWord = (ec ^ 0x13371337) >> 16;
            uint32_t tidHigh = trainerID32 >> 16;
            uint32_t tidLow = trainerID32 & 0xFFFF;

            // Try different high words (start with base, then try variations)
            for (int attempt = 0; attempt < 256; attempt++) {
                uint32_t highWord = (baseHighWord + attempt) & 0xFFFF;
                uint32_t H = highWord ^ tidHigh;

                // Try xorResult values 0-15 (all shiny values, prefer 1 for star shiny)
                for (int targetXor = 1; targetXor < 16; targetXor++) {
                    uint32_t L = H ^ targetXor;
                    uint32_t lowWord = L ^ tidLow;

                    // Check if this preserves gender byte
                    if ((lowWord & 0xFF) == genderByte) {
                        // Found a match! Construct new PID
                        uint32_t newPID = (highWord << 16) | lowWord;

                        // Write new PID and refresh checksum
                        writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
                        refreshChecksum();
                        return;
                    }
                }

                // Also try targetXor = 0 (square shiny)
                uint32_t L = H ^ 0;
                uint32_t lowWord = L ^ tidLow;
                if ((lowWord & 0xFF) == genderByte) {
                    uint32_t newPID = (highWord << 16) | lowWord;
                    writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
                    refreshChecksum();
                    return;
                }
            }

            // If we get here, couldn't find a match (extremely unlikely)
            // Fallback: just make shiny without preserving gender
            uint32_t highWord = baseHighWord & 0xFFFF;
            uint32_t H = highWord ^ tidHigh;
            uint32_t L = H ^ 1; // Star shiny
            uint32_t lowWord = L ^ tidLow;
            uint32_t newPID = (highWord << 16) | lowWord;
            writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
            refreshChecksum();

        } else {
            /**
             * Make Pokemon non-shiny while preserving gender byte.
             *
             * Algorithm:
             * 1. Generate a new PID from EC
             * 2. Preserve the gender byte
             * 3. If still shiny, flip bits until XOR >= 16
             */

            uint32_t ec = encryptionConstant();
            uint32_t newPID = (ec ^ 0x13371337);

            // Preserve gender byte
            newPID = (newPID & 0xFFFFFF00) | genderByte;

            // Check if it's shiny
            uint32_t xorComponent = newPID ^ trainerID32;
            uint32_t xorResult = (xorComponent ^ (xorComponent >> 16)) & 0xFFFF;

            if (xorResult < 16) {
                // Need to make non-shiny - flip bit 8 to change XOR
                newPID ^= 0x00000100;

                // Verify it's now non-shiny
                xorComponent = newPID ^ trainerID32;
                xorResult = (xorComponent ^ (xorComponent >> 16)) & 0xFFFF;

                if (xorResult < 16) {
                    // Still shiny, flip a high bit instead
                    newPID ^= 0x00010000;
                }
            }

            // Write new PID and refresh checksum
            writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x1C), newPID);
            refreshChecksum();
        }
    }
}
