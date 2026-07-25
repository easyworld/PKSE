/**
 * Pokemon3FRLG.cpp - Gen 3 (GBA FRLG) PK3 entity: derived fields, stats, names, PID-based setters.
 * See Pokemon3FRLG.h. Base-stat / species-name / growth-rate tables are reused from the Gen 8/9 data
 * (species constants are stable across generations).
 */
#include "Pokemon/Pokemon3FRLG.h"

#include "Pokemon/BaseStatsGen89.h"     // getBaseStatsGen89 / getSpeciesNameGen89
#include "Pokemon/PersonalInfoTable.h"  // getPersonalInfo (gender ratio + abilities)
#include "Pokemon/Experience.h"         // getLevelFromExp / getExpForLevel / getGrowthRate

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
        // Gen 3 English character table (subset covering names: space, digits, A-Z, a-z). Unmapped -> '?'.
        inline char16_t g3char(uint8_t b) {
            if (b == 0x00) return u' ';
            if (b >= 0xA1 && b <= 0xAA) return static_cast<char16_t>(u'0' + (b - 0xA1));
            if (b >= 0xBB && b <= 0xD4) return static_cast<char16_t>(u'A' + (b - 0xBB));
            if (b >= 0xD5 && b <= 0xEE) return static_cast<char16_t>(u'a' + (b - 0xD5));
            if (b == 0xAB) return u'!';
            if (b == 0xAC) return u'?';
            if (b == 0xAD) return u'.';
            if (b == 0xAE) return u'-';
            return u'?';
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
        const PersonalInfo& pi = getPersonalInfo(speciesID(), form());
        return ((iv32() >> 31) & 1) ? pi.ability2 : pi.ability1;
    }

    std::u16string Pokemon3FRLG::nickname() const {
        std::u16string s;
        for (int i = 0; i < 10; ++i) { const uint8_t b = rd8(0x08 + i); if (b == 0xFF) break; s += g3char(b); }
        return s;
    }
    std::u16string Pokemon3FRLG::otName() const {
        std::u16string s;
        for (int i = 0; i < 7; ++i) { const uint8_t b = rd8(0x14 + i); if (b == 0xFF) break; s += g3char(b); }
        return s;
    }

    // Encode an ASCII-ish name into the Gen 3 character table at dst[offset..]: up to maxChars glyphs,
    // then a 0xFF terminator IF there is room. The PK3 name fields have NO space past maxChars (nickname
    // 10 @ 0x08, OT name 7 @ 0x14 -- 0x1B right after it is Markings), so the terminator is only written
    // when the name is short. Unrepresentable chars end the name; bytes past the terminator are left
    // untouched. Without these setters a created mon's OT name + nickname were BLANK, so Gen 3 read the
    // mon as traded-in ("Apparently met") and showed no name.
    static void g3EncodeName(std::byte* dst, size_t offset, const std::u16string& value, size_t maxChars) {
        size_t n = 0;
        for (char16_t c : value) {
            if (n >= maxChars) break;
            uint8_t b;
            if (c == u' ')                    b = 0x00;
            else if (c >= u'0' && c <= u'9')  b = static_cast<uint8_t>(0xA1 + (c - u'0'));
            else if (c >= u'A' && c <= u'Z')  b = static_cast<uint8_t>(0xBB + (c - u'A'));
            else if (c >= u'a' && c <= u'z')  b = static_cast<uint8_t>(0xD5 + (c - u'a'));
            else if (c == u'!')               b = 0xAB;
            else if (c == u'?')               b = 0xAC;
            else if (c == u'.')               b = 0xAD;
            else if (c == u'-')               b = 0xAE;
            else break;                        // no Gen 3 glyph -> terminate here
            dst[offset + n] = static_cast<std::byte>(b);
            ++n;
        }
        if (n < maxChars) dst[offset + n] = static_cast<std::byte>(0xFF);
    }

    void Pokemon3FRLG::setOTName(const std::u16string& value) noexcept {
        g3EncodeName(data.data(), 0x14, value, 7);    // OT name: 7 chars @ 0x14 (0x1B is Markings)
        refreshChecksum();
    }

    void Pokemon3FRLG::setNickname(const std::u16string& value) noexcept {
        g3EncodeName(data.data(), 0x08, value, 10);   // nickname: 10 chars @ 0x08 (no isNicknamed flag)
        refreshChecksum();
    }

    uint8_t Pokemon3FRLG::baseHP()  const noexcept { const auto* b = getBaseStatsGen89(speciesID(), form()); return b ? b->hp  : 1; }
    uint8_t Pokemon3FRLG::baseATK() const noexcept { const auto* b = getBaseStatsGen89(speciesID(), form()); return b ? b->atk : 1; }
    uint8_t Pokemon3FRLG::baseDEF() const noexcept { const auto* b = getBaseStatsGen89(speciesID(), form()); return b ? b->def : 1; }
    uint8_t Pokemon3FRLG::baseSPE() const noexcept { const auto* b = getBaseStatsGen89(speciesID(), form()); return b ? b->spe : 1; }
    uint8_t Pokemon3FRLG::baseSPA() const noexcept { const auto* b = getBaseStatsGen89(speciesID(), form()); return b ? b->spa : 1; }
    uint8_t Pokemon3FRLG::baseSPD() const noexcept { const auto* b = getBaseStatsGen89(speciesID(), form()); return b ? b->spd : 1; }

    uint16_t Pokemon3FRLG::computeStat(int idx) const noexcept {
        const BaseStatsGen89* bs = getBaseStatsGen89(speciesID(), form());
        if (!bs) return 1;
        const int base[6] = { bs->hp, bs->atk, bs->def, bs->spe, bs->spa, bs->spd };  // HP,ATK,DEF,SPE,SPA,SPD
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

    void Pokemon3FRLG::recalculateStats() noexcept {
        if (dataSize < 0x64) return;   // box mon (80 B): battle stats are computed on the fly, none stored
        wr8(0x54, level());
        const uint16_t hp = computeStat(0);
        wr16(0x56, hp); wr16(0x58, hp);                       // current + max HP
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

    void Pokemon3FRLG::rerollPID(int wantShiny, int wantGender, int wantNature) noexcept {
        const uint32_t tid = id32();
        const uint16_t tsv = static_cast<uint16_t>((tid & 0xFFFF) ^ (tid >> 16));
        const uint8_t  gr  = getPersonalInfo(speciesID(), form()).genderRatio;
        uint32_t p = pid();
        for (int i = 0; i < 2000000; ++i) {
            p = p * 0x41C64E6Du + 0x00006073u;                              // walk candidate PIDs
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

    void Pokemon3FRLG::setNature(uint8_t nature) noexcept { rerollPID(-1, gender(), nature % 25); }
    void Pokemon3FRLG::setGender(uint8_t g)      noexcept { rerollPID(-1, g, nature()); }
    void Pokemon3FRLG::setShiny(bool makeShiny, uint32_t) noexcept { rerollPID(makeShiny ? 1 : 0, gender(), nature()); }
    void Pokemon3FRLG::regeneratePID(uint32_t trainerID32) noexcept {
        rerollPID(isShiny(trainerID32, "") ? 1 : 0, gender(), nature());
    }
}
