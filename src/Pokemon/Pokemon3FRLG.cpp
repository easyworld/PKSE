/**
 * Pokemon3FRLG.cpp - Gen 3 (GBA FRLG) PK3 entity: derived fields, stats, names, PID-based setters.
 * See Pokemon3FRLG.h. Species names and growth rates are shared with the Gen 8/9 data (both are
 * stable across generations); BASE STATS are not, and come from Gen 3's own personal table.
 */
#include <algorithm>

#include "Pokemon/Pokemon3FRLG.h"

#include "Pokemon/BaseStatsGen89.h"     // getSpeciesNameGen89 (species names are generation-stable)
#include "Pokemon/PersonalInfoTable.h"  // getPersonalInfo (gender ratio) / getPersonalInfoG3
#include "Pokemon/Experience.h"         // getLevelFromExp / getExpForLevel / getGrowthRate
#include "Utils/Gen3Text.h"             // the Gen 3 character set, shared with Trainer3FRLG + Convert

namespace Pokemon {

    namespace {
        // Nature stat modifier (order Atk,Def,Spe,SpA,SpD): 0 = -10% (90), 1 = neutral (100), 2 = +10% (110).
        constexpr int8_t NAT[25][5] = {
            {1,1,1,1,1},{2,0,1,1,1},{2,1,0,1,1},{2,1,1,0,1},{2,1,1,1,0},
            {0,2,1,1,1},{1,1,1,1,1},{1,2,0,1,1},{1,2,1,0,1},{1,2,1,1,0},
            {0,1,2,1,1},{1,0,2,1,1},{1,1,1,1,1},{1,1,2,0,1},{1,1,2,1,0},
            {0,1,1,2,1},{1,0,1,2,1},{1,1,0,2,1},{1,1,1,1,1},{1,1,1,2,0},
            {0,1,1,1,2},{1,0,1,1,2},{1,1,0,1,2},{1,1,1,0,2},{1,1,1,1,1},
        };
        inline int natureMod(uint8_t nature, int idx /*0=Atk..4=SpD*/) {
            if (nature >= 25 || idx < 0 || idx >= 5) return 100;
            const int m = NAT[nature][idx];
            return m == 0 ? 90 : (m == 2 ? 110 : 100);
        }
    }

    const char* Pokemon3FRLG::species() const noexcept { return getSpeciesNameGen89(speciesID()); }

    uint8_t Pokemon3FRLG::form() const noexcept {
        if (speciesID() == 201) {   // Unown: form derived from PID
            const uint32_t p = pid();
            const uint32_t v = ((p & 0x03000000u) >> 18) | ((p & 0x00030000u) >> 12)
                             | ((p & 0x00000300u) >> 6)  | (p & 0x00000003u);
            return static_cast<uint8_t>(v % 28);
        }
        return 0;
    }

    uint8_t Pokemon3FRLG::level() const noexcept {
        uint8_t lv = getLevelFromExp(exp(), getGrowthRate(speciesID()));
        return lv < 1 ? 1 : (lv > 100 ? 100 : lv);
    }

    uint8_t Pokemon3FRLG::gender() const noexcept {
        const uint8_t gr = getPersonalInfo(speciesID(), form()).genderRatio;
        if (gr == 255) return 2;           // genderless
        if (gr == 254) return 1;           // female-only
        if (gr == 0)   return 0;           // male-only
        return (pid() & 0xFF) < gr ? 1 : 0;
    }

    uint16_t Pokemon3FRLG::ability() const noexcept {
        // Gen 3's own slot pair -- the modern table disagrees for 101 of the 386 Gen 3
        // species (Sableye is Keen Eye alone here, Keen Eye/Stall there), so reading the
        // bit through the modern table names an ability the game would never show.
        const PersonalInfoG3& g3 = getPersonalInfoG3(speciesID());
        return ((iv32() >> 31) & 1) ? g3.ability2 : g3.ability1;
    }

    // A byte with no glyph is SKIPPED, not shown. Substituting '?' for it is what made a real
    // FireRed FARFETCH'D read back as FARFETCH?D -- and worse, made the substitute indistinguishable
    // from the genuine '?' at 0xAC.
    std::u16string Pokemon3FRLG::readG3Name(size_t offset, int maxChars) const {
        std::u16string s;
        for (int i = 0; i < maxChars; ++i) {
            const uint8_t b = rd8(offset + i);
            if (b == Utils::GEN3_TERMINATOR) break;
            if (const char16_t c = Utils::gen3ToChar(b)) s += c;
        }
        return s;
    }

    std::u16string Pokemon3FRLG::nickname() const { return readG3Name(0x08, 10); }
    std::u16string Pokemon3FRLG::otName()   const { return readG3Name(0x14, 7); }

    // Encode a name into the Gen 3 character table at dst[offset..]: up to maxChars glyphs,
    // then a 0xFF terminator IF there is room. The PK3 name fields have NO space past maxChars (nickname
    // 10 @ 0x08, OT name 7 @ 0x14 -- 0x1B right after it is Markings), so the terminator is only written
    // when the name is short. Unrepresentable chars end the name; bytes past the terminator are left
    // untouched. Without these setters a created mon's OT name + nickname were BLANK, so Gen 3 read the
    // mon as traded-in ("Apparently met") and showed no name.
    static void g3EncodeName(std::byte* dst, size_t offset, const std::u16string& value, size_t maxChars) {
        size_t n = 0;
        for (const char16_t c : value) {
            if (n >= maxChars) break;
            const uint8_t b = Utils::charToGen3(c);
            if (b == Utils::GEN3_TERMINATOR) break;   // no Gen 3 glyph -> end the name here
            dst[offset + n] = static_cast<std::byte>(b);
            ++n;
        }
        if (n < maxChars) dst[offset + n] = static_cast<std::byte>(Utils::GEN3_TERMINATOR);
    }

    void Pokemon3FRLG::setOTName(const std::u16string& value) noexcept {
        g3EncodeName(data.data(), 0x14, value, 7);    // OT name: 7 chars @ 0x14 (0x1B is Markings)
        refreshChecksum();
    }

    // Every character must have a byte in the Gen 3 table. Callers check this and refuse the name
    // outright, because g3EncodeName ends the name at the first unmappable character -- which would
    // store a truncated name instead of reporting that it can't be stored.
    bool Pokemon3FRLG::canStoreNickname(const std::u16string& value) const noexcept {
        for (const char16_t c : value) {
            if (Utils::charToGen3(c) == Utils::GEN3_TERMINATOR) return false;
        }
        return true;
    }

    void Pokemon3FRLG::setNickname(const std::u16string& value) noexcept {
        g3EncodeName(data.data(), 0x08, value, 10);   // nickname: 10 chars @ 0x08 (no isNicknamed flag)
        refreshChecksum();
    }

    uint8_t Pokemon3FRLG::baseHP()  const noexcept { return getPersonalInfoG3(speciesID()).hp;  }
    uint8_t Pokemon3FRLG::baseATK() const noexcept { return getPersonalInfoG3(speciesID()).atk; }
    uint8_t Pokemon3FRLG::baseDEF() const noexcept { return getPersonalInfoG3(speciesID()).def; }
    uint8_t Pokemon3FRLG::baseSPE() const noexcept { return getPersonalInfoG3(speciesID()).spe; }
    uint8_t Pokemon3FRLG::baseSPA() const noexcept { return getPersonalInfoG3(speciesID()).spa; }
    uint8_t Pokemon3FRLG::baseSPD() const noexcept { return getPersonalInfoG3(speciesID()).spd; }

    uint16_t Pokemon3FRLG::computeStat(int idx) const noexcept {
        const PersonalInfoG3& bs = getPersonalInfoG3(speciesID());
        const int base[6] = { bs.hp, bs.atk, bs.def, bs.spe, bs.spa, bs.spd };  // HP,ATK,DEF,SPE,SPA,SPD
        // Base HP 0 means "no row for this species" (out of Gen 3's dex, or a blank slot), NOT a
        // real base stat -- computing from it would invent a stat for a mon we know nothing about.
        if (base[0] == 0) return 1;
        // Shedinja: a base HP of 1 is the game's flag for "always 1 HP", not an input to the
        // formula (PKHeX PKM.GetStats). Without this it would come out of the HP formula at ~76.
        if (idx == 0 && base[0] == 1) return 1;
        const int iv[6]   = { ivHP(), ivATK(), ivDEF(), ivSPE(), ivSPA(), ivSPD() };
        const int ev[6]   = { evHP(), evATK(), evDEF(), evSPE(), evSPA(), evSPD() };
        const int lv = level();
        const int common = (2 * base[idx] + iv[idx] + ev[idx] / 4) * lv / 100;
        if (idx == 0) return static_cast<uint16_t>(common + lv + 10);                 // HP formula
        return static_cast<uint16_t>((common + 5) * natureMod(nature(), idx - 1) / 100);
    }

    uint16_t Pokemon3FRLG::statHPMax() const noexcept { return computeStat(0); }
    uint16_t Pokemon3FRLG::statATK()   const noexcept { return computeStat(1); }
    uint16_t Pokemon3FRLG::statDEF()   const noexcept { return computeStat(2); }
    uint16_t Pokemon3FRLG::statSPE()   const noexcept { return computeStat(3); }
    uint16_t Pokemon3FRLG::statSPA()   const noexcept { return computeStat(4); }
    uint16_t Pokemon3FRLG::statSPD()   const noexcept { return computeStat(5); }
    uint16_t Pokemon3FRLG::statHPCurrent() const noexcept {
        if (dataSize < 0x64) return statHPMax();
        return rd16(0x56);
    }

    void Pokemon3FRLG::setStatHPCurrent(uint16_t value) noexcept {
        if (dataSize < 0x64) return;   // box record has no such field
        wr16(0x56, std::min<uint16_t>(value, statHPMax()));
        // 0x56 sits past the checksummed region (0x20..0x4F), so no refreshChecksum() is needed.
    }

    uint16_t Pokemon3FRLG::carryCurrentHP(uint16_t storedCur, uint16_t storedMax,
                                          uint16_t newMax) noexcept {
        if (storedMax != newMax) return newMax;   // max moved (or was never set) -> refill
        return std::min(storedCur, newMax);       // max unchanged -> keep the mon's own HP
    }

    void Pokemon3FRLG::recalculateStats() noexcept {
        if (dataSize < 0x64) return;   // box mon (80 B): battle stats are computed on the fly, none stored
        wr8(0x54, level());
        const uint16_t max = computeStat(0);
        wr16(0x56, carryCurrentHP(rd16(0x56), rd16(0x58), max));
        wr16(0x58, max);
        wr16(0x5A, computeStat(1)); wr16(0x5C, computeStat(2));  // ATK / DEF
        wr16(0x5E, computeStat(3)); wr16(0x60, computeStat(4));  // SPE / SPA
        wr16(0x62, computeStat(5));                            // SPD
    }

    void Pokemon3FRLG::setLevel(uint8_t lv) noexcept {
        if (lv < 1) lv = 1; else if (lv > 100) lv = 100;
        wr32(0x24, getExpForLevel(lv, getGrowthRate(speciesID())));
        recalculateStats();
        refreshChecksum();
    }

    namespace {
        // Unown's LETTER is PID-derived too: four 2-bit fields (bits 0-1, 8-9, 16-17, 24-25) form an
        // 8-bit value, and the letter is that value % 28.
        constexpr uint32_t UNOWN_FORM_BITS = 0x03030303u;
        inline uint32_t unownFormValue(uint32_t p) noexcept {
            return ((p & 0x03000000u) >> 18) | ((p & 0x00030000u) >> 12)
                 | ((p & 0x00000300u) >> 6)  | (p & 0x00000003u);
        }
        // Inverse: scatter an 8-bit value back into those four fields.
        inline uint32_t withUnownFormValue(uint32_t p, uint32_t v) noexcept {
            return (p & ~UNOWN_FORM_BITS)
                 | ((v & 0xC0u) << 18) | ((v & 0x30u) << 12) | ((v & 0x0Cu) << 6) | (v & 0x03u);
        }
    }

    void Pokemon3FRLG::rerollPID(int wantShiny, int wantGender, int wantNature, int wantAbilityBit) noexcept {
        const uint32_t tid = id32();
        const uint16_t tsv = static_cast<uint16_t>((tid & 0xFFFF) ^ (tid >> 16));
        const uint8_t  gr  = getPersonalInfo(speciesID(), form()).genderRatio;
        uint32_t p = pid();

        // Unown: the letter is part of the PID, so a reroll silently renamed the Pokemon -- editing
        // shininess or nature turned an UNOWN A into an UNOWN F. It is always preserved; no caller
        // wants it changed (setForm is a no-op in Gen 3 precisely because the PID owns it).
        //
        // CONSTRUCTED, not filtered. Rejecting 27 of every 28 candidates would multiply an already
        // expensive search (a shiny nature change is ~1 PID in 819k) by 28 and blow the budget. Instead
        // the letter's bits are overwritten on each candidate with a pattern that already yields the
        // wanted letter, so every candidate tested is a letter match and the search costs what it did
        // before. The other 24 bits still come from the walk, so nature/gender/shiny stay uniform.
        const bool isUnown = (speciesID() == 201);
        const int  wantLetter = isUnown ? static_cast<int>(form()) : -1;
        uint32_t letterPatterns[10] = {};   // 8-bit values v with v % 28 == wantLetter (at most 10)
        int letterPatternCount = 0;
        if (isUnown) {
            for (uint32_t v = static_cast<uint32_t>(wantLetter); v <= 0xFFu; v += 28)
                letterPatterns[letterPatternCount++] = v;
            // Every valid pattern shares the letter's parity (28 is even), so bit 0 -- and with it the
            // Gen 3 ability bit -- is decided by the letter. Preserving both is therefore consistent by
            // construction: the current PID already satisfies it. Nothing to reconcile.
        }
        // Budget: the hardest real case is a SHINY mon of a dual-ability species having its nature
        // changed -- shiny (1/8192) x nature (1/25) x gender (1/2) x ability bit (1/2) is roughly one
        // PID in 819k. Measured over 80 random shiny mons, 2M candidates left 10% of them unable to
        // stay shiny while 8M left none (worst successful search: 3.45M). A miss is only ever paid
        // once, on a button press, and each step is a multiply-add plus a few compares.
        uint32_t walk = p;
        for (int i = 0; i < 8000000; ++i) {
            walk = walk * 0x41C64E6Du + 0x00006073u;                        // walk candidate PIDs
            p = walk;
            // Stamp in a letter-preserving bit pattern before ANY test -- every constraint below reads
            // the whole PID, so this has to happen first or they would be judging a PID we won't store.
            if (letterPatternCount > 0)
                p = withUnownFormValue(p, letterPatterns[(walk >> 2) % static_cast<uint32_t>(letterPatternCount)]);
            // Cheapest reject first: this LCG's low bit flips every step, so half the walk is
            // discarded by an AND before the nature modulo runs.
            if (wantAbilityBit >= 0 && static_cast<int>(p & 1u) != wantAbilityBit) continue;
            if (wantNature >= 0 && static_cast<int>(p % 25) != wantNature) continue;
            if (wantGender >= 0) {
                const int g = (gr == 255) ? 2 : (gr == 254) ? 1 : (gr == 0) ? 0 : ((p & 0xFF) < gr ? 1 : 0);
                if (g != wantGender) continue;
            }
            if (wantShiny >= 0) {
                const uint16_t psv = static_cast<uint16_t>((p & 0xFFFF) ^ (p >> 16));
                const bool sh = ((tsv ^ psv) < 8);
                if (sh != (wantShiny != 0)) continue;
            }
            wr32(0x00, p); recalculateStats(); refreshChecksum();
            return;
        }
        // no PID satisfied the constraints within the budget -> leave the mon unchanged
    }

    void Pokemon3FRLG::rerollPreservingShiny(int wantGender, int wantNature, int wantAbilityBit) noexcept {
        const int wasShiny = isShiny(id32(), "") ? 1 : 0;
        const uint32_t before = pid();
        rerollPID(wasShiny, wantGender, wantNature, wantAbilityBit);
        // The walk steps before its first test and this LCG is full-period, so it cannot return to
        // its own start inside the budget: an unchanged PID means the search failed, not that it
        // found the PID we already had. Shininess is far and away the most expensive constraint, so
        // it is the one dropped on the retry -- the edit the user actually asked for still lands.
        if (pid() == before)
            rerollPID(-1, wantGender, wantNature, wantAbilityBit);
    }

    // Ability slot 1/2. Gen 3 keeps the choice in IV32 bit 31, but legality also wants the
    // PID's low bit to agree with it (PKHeX AbilityVerifier.GetPIDAbilityMatch), so the bit
    // write is paired with a PID re-roll -- exactly PKHeX's SetAbilityIndex, which calls
    // EntityPID.GetRandomPID and then RefreshAbility.
    void Pokemon3FRLG::setAbilityNumber(uint8_t number) noexcept {
        const uint16_t sp = speciesID();
        // Granbull / Vibrava / Flygon: PKHeX's Gen 3 personal data collapses their second
        // ability onto the first, but the real games still let the bit be set -- so leave an
        // existing bit alone rather than clearing it (PKHeX G3PKM.RefreshAbility does the same).
        // Vibrava and Flygon inherit the bit from the Trapinch they evolved from, which is where
        // a legitimately-set bit on a species with one ability comes from. Trapinch itself (328)
        // is NOT in this set -- it has a real Hyper Cutter / Arena Trap choice.
        if (sp == 210 || sp == 329 || sp == 330) return;

        const PersonalInfoG3& g3 = getPersonalInfoG3(sp);

        // Single-ability species: PKHeX only requires the slot bit to be CLEAR for these
        // (AbilityVerifier's IsAbility12Same branch) and asks nothing of the PID, since both slots
        // resolve to the same ability either way -- so clear the bit without re-rolling. That also
        // protects Unown, whose LETTER is derived from the PID: re-rolling to satisfy an ability it
        // only has one of would silently change which Unown it is.
        if (g3.ability2 == g3.ability1) {
            const uint32_t iv = iv32();
            if (iv & (1u << 31)) { wr32(0x48, iv & ~(1u << 31)); refreshChecksum(); }
            return;
        }

        const int wantBit = (number == 2) ? 1 : 0;
        // Already in the wanted state, PID included -> change nothing. A mon whose PID and bit
        // DISAGREE still falls through, so re-picking the ability is also how that gets repaired.
        if (static_cast<int>((iv32() >> 31) & 1) == wantBit && static_cast<int>(pid() & 1u) == wantBit)
            return;

        uint32_t iv = iv32();
        if (wantBit) iv |= (1u << 31); else iv &= ~(1u << 31);
        wr32(0x48, iv);

        // Keep nature, gender and shininess; only the ability bit moves.
        if (static_cast<int>(pid() & 1u) != wantBit)
            rerollPreservingShiny(gender(), nature(), wantBit);
        refreshChecksum();
    }

    void Pokemon3FRLG::setAbility(uint16_t abilityValue) noexcept {
        // A PK3 has no ability id field, so only the species' own two slots are expressible.
        // An id that fills neither is silently ignored rather than written somewhere wrong.
        const PersonalInfoG3& g3 = getPersonalInfoG3(speciesID());
        if (abilityValue == g3.ability1)      setAbilityNumber(1);
        else if (abilityValue == g3.ability2) setAbilityNumber(2);
    }

    int Pokemon3FRLG::abilityPidBit() const noexcept {
        // Only a species with a real ability choice ties its PID's low bit to the stored slot bit;
        // for a single-ability species PKHeX asks nothing of the PID (AbilityVerifier only wants the
        // bit clear), so leaving it unconstrained keeps the search twice as fast -- and keeps an
        // Unown's letter reachable, which an extra pinned bit would make harder.
        const PersonalInfoG3& g3 = getPersonalInfoG3(speciesID());
        if (g3.ability2 == g3.ability1) return -1;
        return static_cast<int>((iv32() >> 31) & 1);
    }

    // Nature, gender, shininess and the ability bit all live in the same PID, so changing one of
    // them means re-deriving all four. Each setter pins the three it was NOT asked to change --
    // editing a nature must not silently un-shiny the mon, nor desync its ability from its PID.
    void Pokemon3FRLG::setNature(uint8_t nature) noexcept {
        rerollPreservingShiny(gender(), nature % 25, abilityPidBit());
    }
    void Pokemon3FRLG::setGender(uint8_t g) noexcept {
        rerollPreservingShiny(g, nature(), abilityPidBit());
    }

    // setShiny is the one setter shininess is the SUBJECT of, so it is pinned rather than preserved.
    void Pokemon3FRLG::setShiny(bool makeShiny, uint32_t) noexcept {
        rerollPID(makeShiny ? 1 : 0, gender(), nature(), abilityPidBit());
    }

    // Legality touch-up after an IV/EV edit -- nothing about the mon was asked to change, so unlike
    // the setters above this does NOT fall back to dropping shininess. If no PID can hold all four,
    // keeping the old PID is the safe failure; quietly un-shinying a mon the user never edited is not.
    void Pokemon3FRLG::regeneratePID(uint32_t trainerID32) noexcept {
        rerollPID(isShiny(trainerID32, "") ? 1 : 0, gender(), nature(), abilityPidBit());
    }
}
