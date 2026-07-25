/**
 * Pokemon3FRLG.h - Generation 3 (GBA FireRed/LeafGreen) Pokemon entity (PK3).
 *
 * Wraps a DECRYPTED, canonically-ordered PK3 buffer (see Encryption3FRLG): 32-byte header +
 * four 12-byte substructures G/A/E/M at fixed offsets 0x20/0x2C/0x38/0x44 + party stats. Gen 3
 * stores no nature / gender / ability / level -- those are derived from the PID (+ the personal
 * table). Species @0x20 is the Gen 3 INTERNAL id; speciesID() returns the National id.
 * Offsets: docs/ROADMAP_TO_V1.md App. A (PKHeX PK3.cs / PokeCrypto.cs).
 */
#ifndef POKEMON_POKEMON3_FRLG_H
#define POKEMON_POKEMON3_FRLG_H

#include <cstdint>
#include <span>
#include <string>

#include "Pokemon/Pokemon.h"
#include "Encryption/Encryption3FRLG.h"

namespace Pokemon {

    // Gen 3 internal <-> National dex species id. 1-251 are identical; Hoenn (National 252-386)
    // lives at internal 277-411 (a uniform +25 shift after 25 unused internal slots).
    inline uint16_t g3ToNational(uint16_t internalId) {
        if (internalId < 252) return internalId;
        if (internalId >= 277 && internalId <= 411) return static_cast<uint16_t>(internalId - 25);
        return 0;   // unused Gen 3 internal slot
    }
    inline uint16_t nationalToG3(uint16_t national) {
        if (national < 252) return national;
        if (national <= 386) return static_cast<uint16_t>(national + 25);
        return 0;   // not obtainable in Gen 3
    }

    class Pokemon3FRLG final : public Pokemon {
    public:
        explicit Pokemon3FRLG(std::span<const std::byte> raw) {
            buffer = Encryption::decryptArray3FRLG(raw);   // decrypt + un-shuffle to canonical G/A/E/M
            dataSize = raw.size();
            data = std::span<std::byte>(buffer, dataSize);
        }
        ~Pokemon3FRLG() override = default;
        Pokemon3FRLG(const Pokemon3FRLG&) = delete;
        Pokemon3FRLG& operator=(const Pokemon3FRLG&) = delete;
        Pokemon3FRLG(Pokemon3FRLG&&) noexcept = default;
        Pokemon3FRLG& operator=(Pokemon3FRLG&&) noexcept = default;

        std::unique_ptr<Pokemon> clone() const override {
            std::byte* enc = Encryption::encryptArray3FRLG(std::span<const std::byte>(data.data(), dataSize));
            auto c = std::make_unique<Pokemon3FRLG>(std::span<const std::byte>(enc, dataSize));
            delete[] enc;
            return c;
        }

        Enums::GameVersion getGameGroup() const noexcept override { return Enums::GameVersion::FRLG; }

        // ---- byte helpers (canonical, decrypted buffer) ----
        uint8_t  rd8(size_t o)  const noexcept { return static_cast<uint8_t>(data[o]); }
        uint16_t rd16(size_t o) const noexcept { return static_cast<uint16_t>(rd8(o)) | (static_cast<uint16_t>(rd8(o + 1)) << 8); }
        uint32_t rd32(size_t o) const noexcept { return static_cast<uint32_t>(rd16(o)) | (static_cast<uint32_t>(rd16(o + 2)) << 16); }
        void wr8(size_t o, uint8_t v)  noexcept { data[o] = static_cast<std::byte>(v); }
        void wr16(size_t o, uint16_t v) noexcept { wr8(o, static_cast<uint8_t>(v)); wr8(o + 1, static_cast<uint8_t>(v >> 8)); }
        void wr32(size_t o, uint32_t v) noexcept { wr16(o, static_cast<uint16_t>(v)); wr16(o + 2, static_cast<uint16_t>(v >> 16)); }

        // ---- core identity ----
        uint32_t pid() const noexcept override { return rd32(0x00); }
        uint32_t encryptionConstant() const noexcept override { return rd32(0x00); }  // Gen3 has no EC; PID is the seed
        uint16_t speciesID() const noexcept override { return g3ToNational(rd16(0x20)); }
        const char* species() const noexcept override;
        uint8_t formID() const noexcept override { return form(); }
        uint8_t form() const noexcept override;   // Unown (201) form from PID; else 0
        uint16_t heldItem() const noexcept override { return rd16(0x22); }  // Gen3 item id
        uint32_t id32() const noexcept override { return rd32(0x04); }
        uint16_t tid16() const noexcept override { return rd16(0x04); }
        uint16_t sid16() const noexcept override { return rd16(0x06); }
        uint32_t exp() const noexcept override { return rd32(0x24); }
        uint8_t nature() const noexcept override { return static_cast<uint8_t>(pid() % 25); }
        uint8_t statNature() const noexcept override { return nature(); }   // no mints in Gen3
        uint8_t level() const noexcept override;   // derived from EXP
        uint8_t gender() const noexcept override;  // derived from PID + species ratio
        const char* genderSymbol() const noexcept override {
            const uint8_t g = gender();
            return g == 0 ? "♂" : g == 1 ? "♀" : "";
        }

        // Ability: Gen3 stores a selector bit (IV32 bit31); the id comes from the personal table.
        uint16_t ability() const noexcept override;
        uint8_t abilityNumber() const noexcept override { return (rd32(0x48) >> 31) & 1 ? 2 : 1; }

        std::u16string nickname() const override;   // Gen3 char table @0x08 (10 bytes)
        void setNickname(const std::u16string& value) noexcept override;   // encode into the Gen3 table @0x08
        std::u16string otName() const override;      // Gen3 char table @0x14 (7 bytes)
        void setOTName(const std::u16string& value) noexcept override;   // encode into the Gen3 table @0x14

        // ---- moves (Attacks substructure @0x2C) ----
        uint16_t move(int slot) const noexcept override { return (slot < 0 || slot > 3) ? 0 : rd16(0x2C + slot * 2); }
        uint8_t movePP(int slot) const noexcept override { return (slot < 0 || slot > 3) ? 0 : rd8(0x34 + slot); }
        uint8_t movePPUps(int slot) const noexcept override { return (slot < 0 || slot > 3) ? 0 : (rd8(0x28) >> (slot * 2)) & 0x03; }
        void setMove(int slot, uint16_t m) noexcept override { if (slot >= 0 && slot <= 3) { wr16(0x2C + slot * 2, m); refreshChecksum(); } }
        void setMovePP(int slot, uint8_t pp) noexcept override { if (slot >= 0 && slot <= 3) { wr8(0x34 + slot, pp); refreshChecksum(); } }
        void setMovePPUps(int slot, uint8_t up) noexcept override {
            if (slot < 0 || slot > 3) return;
            uint8_t b = (rd8(0x28) & ~(0x03 << (slot * 2))) | ((up & 0x03) << (slot * 2));
            wr8(0x28, b); refreshChecksum();
        }

        // ---- IVs (packed IV32 @0x48) ----
        uint32_t iv32() const noexcept { return rd32(0x48); }
        uint8_t ivHP()  const noexcept override { return (iv32() >> 0) & 0x1F; }
        uint8_t ivATK() const noexcept override { return (iv32() >> 5) & 0x1F; }
        uint8_t ivDEF() const noexcept override { return (iv32() >> 10) & 0x1F; }
        uint8_t ivSPE() const noexcept override { return (iv32() >> 15) & 0x1F; }
        uint8_t ivSPA() const noexcept override { return (iv32() >> 20) & 0x1F; }
        uint8_t ivSPD() const noexcept override { return (iv32() >> 25) & 0x1F; }
        void setIV(int statIndex, uint8_t v) noexcept override {
            if (statIndex < 0 || statIndex >= 6 || v > 31) return;
            uint32_t iv = iv32();
            const int shift = statIndex * 5;
            iv = (iv & ~(0x1Fu << shift)) | (static_cast<uint32_t>(v & 0x1F) << shift);
            wr32(0x48, iv); refreshChecksum();
        }

        // ---- EVs (EVs/Condition substructure @0x38) ----
        uint8_t evHP()  const noexcept override { return rd8(0x38); }
        uint8_t evATK() const noexcept override { return rd8(0x39); }
        uint8_t evDEF() const noexcept override { return rd8(0x3A); }
        uint8_t evSPE() const noexcept override { return rd8(0x3B); }
        uint8_t evSPA() const noexcept override { return rd8(0x3C); }
        uint8_t evSPD() const noexcept override { return rd8(0x3D); }
        void setEV(int statIndex, uint8_t v) noexcept override {
            if (statIndex >= 0 && statIndex < 6) { wr8(0x38 + statIndex, v); refreshChecksum(); }
        }

        // ---- OT / origin / met (Misc substructure @0x44 + header) ----
        uint16_t origins() const noexcept { return rd16(0x46); }
        uint8_t originGame() const noexcept override { return (origins() >> 7) & 0x0F; }
        uint8_t metLevel() const noexcept override { return origins() & 0x7F; }
        uint8_t otGender() const noexcept override { return (origins() >> 15) & 0x01; }
        uint16_t metLocation() const noexcept override { return rd8(0x45); }
        uint8_t ball() const noexcept override { return (origins() >> 11) & 0x0F; }
        uint8_t otFriendship() const noexcept override { return rd8(0x29); }
        uint8_t language() const noexcept override { return rd8(0x12); }
        uint8_t friendship() const noexcept override { return rd8(0x29); }
        uint8_t pokerus() const noexcept { return rd8(0x44); }
        bool isPokerusInfected() const noexcept override { return (pokerus() & 0x0F) != 0; }
        bool isPokerusCured() const noexcept override { return (pokerus() & 0xF0) != 0 && (pokerus() & 0x0F) == 0; }
        void setPokerus(uint8_t value) noexcept override { wr8(0x44, value); refreshChecksum(); }
        bool hasPokerus() const noexcept override { return true; }

        bool isEgg() const noexcept override { return ((iv32() >> 30) & 1) != 0; }
        bool isShiny(uint32_t trainerID32, std::string) const noexcept override {
            const uint32_t p = pid();
            const uint16_t tsv = static_cast<uint16_t>((trainerID32 & 0xFFFF) ^ (trainerID32 >> 16));
            const uint16_t psv = static_cast<uint16_t>((p & 0xFFFF) ^ (p >> 16));
            return (tsv ^ psv) < 8;
        }

        // ---- base + battle stats ----
        uint8_t baseHP()  const noexcept override;
        uint8_t baseATK() const noexcept override;
        uint8_t baseDEF() const noexcept override;
        uint8_t baseSPE() const noexcept override;
        uint8_t baseSPA() const noexcept override;
        uint8_t baseSPD() const noexcept override;
        uint16_t statHPMax() const noexcept override;
        uint16_t statATK() const noexcept override;
        uint16_t statDEF() const noexcept override;
        uint16_t statSPE() const noexcept override;
        uint16_t statSPA() const noexcept override;
        uint16_t statSPD() const noexcept override;

        // ---- checksum ----
        uint16_t checksum() const noexcept override { return rd16(0x1C); }
        uint16_t calculateChecksum() const noexcept override { return Encryption::checksum3FRLG(std::span<const std::byte>(data.data(), dataSize)); }
        void refreshChecksum() noexcept override { wr16(0x1C, calculateChecksum()); }
        bool checksumValid() const noexcept override { return checksum() == calculateChecksum(); }

        // ---- editable setters (direct fields; PID-derived fields regenerate the PID) ----
        void setSpecies(uint16_t national) noexcept override {
            wr16(0x20, nationalToG3(national));
            if (national != 0) wr8(0x13, rd8(0x13) | 0x02);   // set HasSpecies flag
            recalculateStats(); refreshChecksum();
        }
        void setForm(uint8_t) noexcept override {}   // Gen3 forms (Unown/Deoxys) are PID/other-derived
        void setHeldItem(uint16_t item) noexcept override { wr16(0x22, item); refreshChecksum(); }
        void setPID(uint32_t p) noexcept override { wr32(0x00, p); recalculateStats(); refreshChecksum(); }
        void setEncryptionConstant(uint32_t p) noexcept override { setPID(p); }
        void setFriendship(uint8_t v) noexcept override { wr8(0x29, v); refreshChecksum(); }
        void setOTFriendship(uint8_t v) noexcept override { wr8(0x29, v); refreshChecksum(); }
        void setTID16(uint16_t v) noexcept override { wr16(0x04, v); refreshChecksum(); }
        void setSID16(uint16_t v) noexcept override { wr16(0x06, v); refreshChecksum(); }
        void setId32(uint32_t v) noexcept override { wr32(0x04, v); refreshChecksum(); }
        void setLanguage(uint8_t v) noexcept override { wr8(0x12, v); refreshChecksum(); }
        void setBall(uint8_t v) noexcept override { wr16(0x46, (origins() & ~(0x0Fu << 11)) | ((v & 0x0Fu) << 11)); refreshChecksum(); }
        void setMetLevel(uint8_t v) noexcept override { wr16(0x46, (origins() & ~0x7Fu) | (v & 0x7Fu)); refreshChecksum(); }
        void setMetLocation(uint16_t v) noexcept override { wr8(0x45, static_cast<uint8_t>(v)); refreshChecksum(); }
        void setOriginGame(uint8_t v) noexcept override { wr16(0x46, (origins() & ~(0x0Fu << 7)) | ((v & 0x0Fu) << 7)); refreshChecksum(); }
        void setOTGender(uint8_t v) noexcept override { wr16(0x46, (origins() & ~(1u << 15)) | ((v & 1u) << 15)); refreshChecksum(); }
        void setEgg(bool egg) noexcept override {
            uint32_t iv = iv32(); if (egg) iv |= (1u << 30); else iv &= ~(1u << 30);
            wr32(0x48, iv); refreshChecksum();
        }
        void setLevel(uint8_t level) noexcept override;      // writes EXP for the level, recalcs
        void setNature(uint8_t nature) noexcept override;    // re-rolls PID to the nature (keeps gender + shiny)
        void setGender(uint8_t gender) noexcept override;    // re-rolls PID to the gender
        void setShiny(bool makeShiny, uint32_t trainerID32) noexcept override;
        void regeneratePID(uint32_t trainerID32) noexcept override;
        void recalculateStats() noexcept override;

    private:
        // Compute a battle stat (0=HP..5=SPD) from base/IV/EV/level/nature (Gen3 formula).
        uint16_t computeStat(int idx) const noexcept;
        // Re-roll the PID (bounded search) to satisfy the given constraints; each is -1 for "don't care".
        // Used by the nature/gender/shiny setters -- all PID-derived in Gen 3. No-op if none found.
        void rerollPID(int wantShiny, int wantGender, int wantNature) noexcept;
    };
}

#endif  // POKEMON_POKEMON3_FRLG_H
