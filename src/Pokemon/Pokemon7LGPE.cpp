/**
 * Pokemon7LGPE.cpp - Generation 7 Pokemon Let's Go Data Class Implementation
 *
 * Implementation of Pokemon7LGPE class methods including stat calculations,
 * gender determination, and Let's Go-specific features.
 */

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "Pokemon/Pokemon7LGPE.h"
#include "Pokemon/BaseStatsGen7.h"
#include "Pokemon/Experience.h"   // getLevelFromExp / getGrowthRate (real growth-table level)

namespace Trainer {
    // Forward declaration for species name lookup
    extern const char* getSpeciesName(uint16_t speciesId);
}

namespace Pokemon {
    // ========================================
    // Species and Name Lookups
    // ========================================

    const char* Pokemon7LGPE::species() const noexcept
    {
        return Trainer::getSpeciesName(speciesID());
    }

    // ========================================
    // Gender Determination
    // ========================================

    uint8_t Pokemon7LGPE::gender() const noexcept
    {
        /**
         * Gender is a STORED 2-bit field at byte 0x1D (bits 1-2): 0=Male, 1=Female,
         * 2=Genderless. It is independent of the PID and the Encryption Constant, so
         * changing shininess must NOT change gender.
         */
        return (static_cast<uint8_t>(data[0x1D]) >> 1) & 0x03;
    }

    // ========================================
    // Base Stats (Species-Dependent)
    // ========================================

    uint8_t Pokemon7LGPE::baseHP() const noexcept
    {
        return getBaseStatsGen7(speciesID(), form())->hp;
    }

    uint8_t Pokemon7LGPE::baseATK() const noexcept
    {
        return getBaseStatsGen7(speciesID(), form())->atk;
    }

    uint8_t Pokemon7LGPE::baseDEF() const noexcept
    {
        return getBaseStatsGen7(speciesID(), form())->def;
    }

    uint8_t Pokemon7LGPE::baseSPE() const noexcept
    {
        return getBaseStatsGen7(speciesID(), form())->spe;
    }

    uint8_t Pokemon7LGPE::baseSPA() const noexcept
    {
        return getBaseStatsGen7(speciesID(), form())->spa;
    }

    uint8_t Pokemon7LGPE::baseSPD() const noexcept
    {
        return getBaseStatsGen7(speciesID(), form())->spd;
    }

    // ========================================
    // Level Calculation
    // ========================================

    uint8_t Pokemon7LGPE::level() const noexcept
    {
        /**
         * Level is derived from EXP through the SPECIES' growth-rate table, per PKHeX
         * PKM.CurrentLevel = Experience.GetLevel(EXP, PersonalInfo.EXPGrowth).
         *
         * Do not approximate this with cbrt(exp): the cube root is only the Medium-Fast curve, so it
         * silently under-reports the level of every species on one of the other five curves (a
         * Medium-Slow Charmander holding the level-22 threshold of 7577 EXP read as 19). That error
         * then cascades -- every stat getter below multiplies by level(), and the legality checker
         * raises bogus "level does not match EXP" / "met level above current level" flags.
         */
        return getLevelFromExp(exp(), getGrowthRate(speciesID()));
    }

    void Pokemon7LGPE::setLevel(uint8_t level) noexcept
    {
        // Level is derived from EXP, so write this level's minimum total EXP (0x10); the stat getters
        // read level() back from it. Without this override the level picker was a base no-op for LGPE.
        if (level < 1) level = 1;
        if (level > 100) level = 100;
        uint32_t exp = getExpForLevel(level, getGrowthRate(speciesID()));
        writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x10), exp);
        recalculateStats();
        refreshChecksum();
    }

    // ========================================
    // Calculated Stats
    // ========================================

    // Single source of truth for LGPE battle stats: the six display getters below AND recalculateStats()
    // (which writes the stored Level/stat/CP tail the GAME reads) all funnel through here, so PKSE's
    // summary can never drift from what the game shows. Formula per PKHeX PB7:
    //   initial = (IV + 2*Base) * Level / 100
    //   HP   = initial + Level + 10 + AV                         (no nature, no friendship)
    //   else = friendship% * ( nature% * (initial + 5) ) + AV    (friendship% = 100..110; nature% = 90/100/110)
    // idx 0..5 = HP, Atk, Def, Spe, SpA, SpD; the nature-table column is idx-1 in [Atk,Def,Spe,SpA,SpD]
    // order. EVs are intentionally excluded -- LGPE trains via AVs, not EVs.
    uint16_t Pokemon7LGPE::computeStat(int idx) const noexcept
    {
        uint8_t lv = level();
        if (lv == 0 || lv > 100) return 1;

        auto baseStat = [&](int i) -> int {
            switch (i) { case 0: return baseHP(); case 1: return baseATK(); case 2: return baseDEF();
                         case 3: return baseSPE(); case 4: return baseSPA(); default: return baseSPD(); }
        };
        auto ivStat = [&](int i) -> int {
            switch (i) { case 0: return ivHP(); case 1: return ivATK(); case 2: return ivDEF();
                         case 3: return ivSPE(); case 4: return ivSPA(); default: return ivSPD(); }
        };
        auto avStat = [&](int i) -> int {
            switch (i) { case 0: return avHP(); case 1: return avATK(); case 2: return avDEF();
                         case 3: return avSPE(); case 4: return avSPA(); default: return avSPD(); }
        };

        const int initial = (ivStat(idx) + 2 * baseStat(idx)) * lv / 100;
        if (idx == 0) return static_cast<uint16_t>(std::max(1, avStat(0) + initial + lv + 10));

        // Nature table, column order [Atk,Def,Spe,SpA,SpD]: 0 = -10% (90), 1 = neutral (100), 2 = +10% (110).
        static const int8_t kNat[25][5] = {
            {1,1,1,1,1},{2,0,1,1,1},{2,1,0,1,1},{2,1,1,0,1},{2,1,1,1,0},
            {0,2,1,1,1},{1,1,1,1,1},{1,2,0,1,1},{1,2,1,0,1},{1,2,1,1,0},
            {0,1,2,1,1},{1,0,2,1,1},{1,1,1,1,1},{1,1,2,0,1},{1,1,2,1,0},
            {0,1,1,2,1},{1,0,1,2,1},{1,1,0,2,1},{1,1,1,1,1},{1,1,1,2,0},
            {0,1,1,1,2},{1,0,1,1,2},{1,1,0,1,2},{1,1,1,0,2},{1,1,1,1,1},
        };
        const uint8_t nat = nature();
        const int col = (nat < 25) ? kNat[nat][idx - 1] : 1;
        const int natureMod = (col == 0) ? 90 : (col == 2 ? 110 : 100);

        const int friendVal = friendship();
        const int scalar = static_cast<int>(((friendVal / 255.0f / 10.0f) + 1.0f) * 100.0f);  // 100..110
        const int amplified = (initial + 5) * natureMod / 100;
        const int part = scalar * amplified / 100;
        return static_cast<uint16_t>(std::max(1, avStat(idx) + part));
    }

    uint16_t Pokemon7LGPE::statHPMax() const noexcept { return computeStat(0); }

    uint16_t Pokemon7LGPE::statATK() const noexcept { return computeStat(1); }

    uint16_t Pokemon7LGPE::statDEF() const noexcept { return computeStat(2); }

    uint16_t Pokemon7LGPE::statSPE() const noexcept { return computeStat(3); }

    uint16_t Pokemon7LGPE::statSPA() const noexcept { return computeStat(4); }

    uint16_t Pokemon7LGPE::statSPD() const noexcept { return computeStat(5); }

    // ========================================
    // Stat Recalculation
    // ========================================

    void Pokemon7LGPE::recalculateStats() noexcept
    {
        // Let's Go stores Level + battle stats + Combat Power in the (un-checksummed) party tail at
        // 0xEC..0xFF. The on-the-fly getters recompute for our display, but the GAME reads these stored
        // bytes -- so a freshly built mon (e.g. a cross-game conversion) needs them written or it shows
        // Level/CP 0. Formula per PKHeX PB7 (LoadStats + CalcCP): the 5 non-HP stats carry the friendship
        // scalar + nature amp + AV; CP = min(10000, BaseCP + AwakeCP).
        const uint16_t species = speciesID();
        uint8_t lvl = getLevelFromExp(exp(), getGrowthRate(species));
        if (lvl < 1) lvl = 1; else if (lvl > 100) lvl = 100;
        data[0xEC] = static_cast<std::byte>(lvl);   // Stat_Level

        // Every stat comes from computeStat() (shared with the display getters) so the stored tail the
        // GAME reads is exactly what PKSE shows -- nature, friendship and AV all included.
        const int hp  = computeStat(0);
        const int atk = computeStat(1);
        const int def = computeStat(2);
        const int spe = computeStat(3);
        const int spa = computeStat(4);
        const int spd = computeStat(5);

        auto put = [&](size_t off, int v) {
            if (v < 1) v = 1; else if (v > 65535) v = 65535;
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + off), static_cast<uint16_t>(v));
        };
        const uint16_t oldMax = readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0xF2));
        put(0xF2, hp);                          // Stat_HPMax
        if (oldMax != static_cast<uint16_t>(hp)) put(0xF0, hp);   // Stat_HPCurrent
        put(0xF4, atk); put(0xF6, def);         // ATK / DEF
        put(0xF8, spe); put(0xFA, spa);         // SPE / SPA
        put(0xFC, spd);                         // SPD

        // CP (PKHeX CalcCP): base part from the NON-AV portion of every stat, plus an AV bonus. Each
        // stat's non-AV portion is (stat - its AV), so statSum = (sum of all six stats) - (sum of AVs).
        const int avSum = avHP() + avATK() + avDEF() + avSPE() + avSPA() + avSPD();
        const int statSum = hp + atk + def + spe + spa + spd - avSum;
        int baseCP = static_cast<int>(static_cast<float>(statSum) * 6.0f * lvl / 100.0f);
        const int awakeCP = (avSum > 0) ? static_cast<int>(avSum * ((lvl * 4.0f / 100.0f) + 2.0f)) : 0;
        int cp = baseCP + awakeCP;
        if (cp > 10000) cp = 10000; else if (cp < 0) cp = 0;
        writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0xFE), static_cast<uint16_t>(cp));  // Stat_CP
    }

    // ========================================
    // Advanced Modification
    // ========================================

    void Pokemon7LGPE::regeneratePID(uint32_t trainerID32) noexcept
    {
        /**
         * Re-seeds the Encryption Constant (0x00). In LGPE the EC is ONLY the crypto seed —
         * PID (0x18), gender (0x1D) and shininess (PID-derived) are all independent of it —
         * so re-seeding is harmless and leaves the Pokemon's identity untouched.
         */
        (void)trainerID32;
        uint32_t oldEC = encryptionConstant();
        uint32_t newEC = (oldEC ^ 0x10101010) + speciesID();
        writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x00), newEC);
        refreshChecksum();
    }

    void Pokemon7LGPE::setShiny(bool makeShiny, uint32_t trainerID32) noexcept
    {
        /**
         * Sets shininess by modifying the PID (0x18) only — NOT the Encryption Constant
         * and NOT the stored gender byte (0x1D). Shiny iff (TID^SID^PIDhi^PIDlo) < 16.
         *
         * That XOR folds the two halves of the PID together, which is the trap this used
         * to fall into: turning a Let's Go Pokemon OFF shiny XOR'd the PID with masks that
         * flipped the SAME bit position in both halves (0x10001000, then 0x01000100). Any
         * such mask cancels itself inside `PIDhi ^ PIDlo` and leaves the shiny value
         * untouched, so the branch could not work and the toggle appeared dead in one
         * direction only. A mask has to be asymmetric between the halves to move it.
         *
         * Rather than flip bits and hope, solve for the value outright. Writing
         *     PIDhi' = PIDhi ^ (current ^ target)
         * makes the new XOR exactly `target`, because the old PIDhi cancels out. Target 1
         * is a star shiny; target 16 is the first non-shiny value, so it is the smallest
         * edit that clears the flag. Only the high word moves, leaving the low byte (the
         * one the other generations guard as their gender byte) untouched.
         */

        if (trainerID32 == 0) {
            return; // Cannot set shiny without trainer ID
        }
        if (isShiny(trainerID32, species()) == makeShiny) {
            return; // Already in the requested state
        }

        const uint32_t p = pid();  // real PID at 0x18
        const uint16_t pidHigh = static_cast<uint16_t>((p >> 16) & 0xFFFF);
        const uint16_t pidLow  = static_cast<uint16_t>(p & 0xFFFF);
        const uint16_t tidHigh = static_cast<uint16_t>((trainerID32 >> 16) & 0xFFFF);
        const uint16_t tidLow  = static_cast<uint16_t>(trainerID32 & 0xFFFF);

        const uint16_t current = static_cast<uint16_t>(pidHigh ^ pidLow ^ tidHigh ^ tidLow);
        const uint16_t target  = makeShiny ? 1 : 16;
        const uint16_t newHigh = static_cast<uint16_t>(pidHigh ^ (current ^ target));

        const uint32_t newPID = (static_cast<uint32_t>(newHigh) << 16) | pidLow;
        writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x18), newPID);
        refreshChecksum();
    }
}
