/**
 * Pokemon7LGPE.h - Generation 7 Pokemon Let's Go Data Class
 *
 * This file defines the Pokemon7LGPE class for Pokemon Let's Go Pikachu/Eevee games.
 *
 * Pokemon7LGPE Format Details:
 * - Fixed Size: 260 bytes (0x104) for both party and stored
 * - Encryption: XOR cipher + block shuffling (Gen 6/7 algorithm)
 * - Data Layout: 4 blocks (56 bytes each) + additional data
 *
 * Key Differences from PK8:
 * - Smaller block size (56 bytes vs 80 bytes)
 * - No separate party stats section (all data is within the 260 bytes)
 * - Awakening Values (AVs) for stat bonuses (unique to Let's Go)
 * - Combat Power (CP) stat (similar to Pokemon GO)
 * - Height/Weight absolute values (actual measurements)
 * - Simplified data structure
 *
 * Data Structure (Decrypted):
 * 0x00-0x07: Header (Encryption Constant, Checksum)
 * 0x08-0x3F: Block A (Growth) - Species, items, nature, AVs, stats
 * 0x40-0x77: Block B (Attacks) - Moves, IVs, nickname
 * 0x78-0xAF: Block C (Trainer) - OT info, memories
 * 0xB0-0xE7: Block D (Misc) - Met info, ribbons
 * 0xE8-0x103: Extended data - CP, weight, height
 */

#ifndef POKEMON_POKEMON7_LGPE_H
#define POKEMON_POKEMON7_LGPE_H

#include <cstdint>
#include <cstring>
#include <span>
#include <string>

#include "Pokemon/Pokemon.h"
#include "Encryption/Encryption7LGPE.h"
#include "Utils/HelperUtilities.h"
#include "Utils/StringHelpers.h"

using namespace Encryption;
using namespace Pokemon;
using namespace Utils;

namespace Pokemon {
    /**
     * Pokemon7LGPE - Pokemon Let's Go Pikachu/Eevee Pokemon Class
     *
     * Inherits from Pokemon base class and implements Let's Go-specific data format.
     * Handles automatic decryption on construction and provides accessors for
     * all Pokemon properties.
     *
     * Let's Go Unique Features:
     * - Awakening Values (AVs): Stat bonuses earned through catching Pokemon (0-200 per stat)
     * - Combat Power (CP): Overall power rating like Pokemon GO
     * - Height/Weight Scalars: Individual size variations
     * - Absolute Height/Weight: Actual measurements in meters/kilograms
     * - Simpler structure than modern generations
     */
    class Pokemon7LGPE final : public Pokemon
    {
    public:
        /**
         * Constructs a Pokemon7LGPE object from encrypted Pokemon data.
         *
         * Process:
         * 1. Decrypts the data using Gen7 decryption algorithm
         * 2. Stores decrypted data in internal buffer
         * 3. Creates span view for easy access
         *
         * @param raw Encrypted Pokemon data (SIZE_PK7 = 260 bytes)
         */
        explicit Pokemon7LGPE(std::span<const std::byte> raw)
        {
            // Decrypt the Gen 7 Pokemon data
            buffer = decryptArray7LGPE(raw);
            dataSize = raw.size();
            data = std::span<std::byte>(buffer, dataSize);
        }

        /**
         * Destructor - cleans up decrypted data buffer.
         * The base class Pokemon destructor handles buffer cleanup.
         */
        ~Pokemon7LGPE() override = default;

        // Prevent copying (Pokemon data should not be accidentally copied)
        Pokemon7LGPE(const Pokemon7LGPE&) = delete;
        Pokemon7LGPE& operator=(const Pokemon7LGPE&) = delete;

        // Allow moving for efficient transfers
        Pokemon7LGPE(Pokemon7LGPE&&) noexcept = default;
        Pokemon7LGPE& operator=(Pokemon7LGPE&&) noexcept = default;

        /** Deep-copy: re-encrypt the decrypted buffer and rebuild via the encrypted-span ctor.
         *  Offset 0x00 is the EC/crypto seed (the PID lives separately at 0x18 in LGPE). */
        std::unique_ptr<Pokemon> clone() const override {
            uint32_t ec = readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data()));
            std::byte* enc = encryptArray7LGPE(std::span<const std::byte>(data.data(), dataSize), ec);
            auto c = std::make_unique<Pokemon7LGPE>(std::span<const std::byte>(enc, dataSize));
            delete[] enc;
            return c;
        }

        /** Storage-format game group (this subclass), NOT the origin Version byte. */
        Enums::GameVersion getGameGroup() const noexcept override { return Enums::GameVersion::GG; }

        // ========================================
        // Core Data Properties (Block A - Growth)
        // ========================================

        /**
         * Gets the Pokemon's Species ID.
         * Location: 0x08 (2 bytes)
         * @return Species ID (e.g., 25 = Pikachu, 133 = Eevee)
         */
        uint16_t speciesID() const noexcept override
        {
            return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x08));
        }

        /** Sets species (0x08) and recalculates stats. Without this override the base no-op left a
         *  freshly-CREATED LGPE mon at species 0, so the details editor -- which bails on species 0 --
         *  never appeared (the create-Pokemon-shows-no-editor bug). */
        void setSpecies(uint16_t species) noexcept override
        {
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x08), species);
            recalculateStats();
            refreshChecksum();
        }

        /** Core-identity setters, also unwired before (base no-ops), so a CREATED LGPE mon had
         *  EC / PID / TID / form all zero. EC 0 is the worst: the save's read path treats a zero-EC
         *  slot as EMPTY, so a created mon would vanish on reload. */
        void setEncryptionConstant(uint32_t ec) noexcept override
        { writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x00), ec); refreshChecksum(); }
        void setPID(uint32_t pidValue) noexcept override
        { writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x18), pidValue); refreshChecksum(); }
        void setId32(uint32_t value) noexcept override
        { writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x0C), value); refreshChecksum(); }
        /** Form lives in the high 5 bits of 0x1D (bit0 = Fateful, bits1-2 = Gender). */
        void setForm(uint8_t formValue) noexcept override
        {
            uint8_t b = (static_cast<uint8_t>(data[0x1D]) & 0x07) | ((formValue & 0x1F) << 3);
            data[0x1D] = static_cast<std::byte>(b);
            recalculateStats();
            refreshChecksum();
        }

        /** Fateful-encounter flag -- bit 0 of 0x1D (the byte also holds gender bits 1-2 / form bits 3-7,
         *  which setGender/setForm leave untouched). */
        bool isFatefulEncounter() const noexcept override { return (static_cast<uint8_t>(data[0x1D]) & 0x01) != 0; }
        void setFatefulEncounter(bool value) noexcept override
        {
            uint8_t b = (static_cast<uint8_t>(data[0x1D]) & 0xFE) | (value ? 0x01 : 0x00);
            data[0x1D] = static_cast<std::byte>(b);
            refreshChecksum();
        }

        /** Level is EXP-derived; setLevel writes the level's minimum EXP (0x10) and recalcs. Defined in
         *  the .cpp (needs the growth-rate / EXP tables). Without it the level picker was a no-op here. */
        void setLevel(uint8_t level) noexcept override;

        /**
         * Gets the Pokemon's species name as a string.
         * @return Species name (e.g., "Pikachu", "Eevee")
         */
        const char* species() const noexcept override;

        /**
         * Gets the Form.
         * Location: 0x1D (1 byte)
         * @return Form ID (0 = no form)
         */
        uint8_t formID() const noexcept override
        {
            // Byte 0x1D packs Fateful(bit0) + Gender(bits1-2) + Form(bits3-7).
            return static_cast<uint8_t>(data[0x1D]) >> 3;
        }

        /**
         * Gets the held item ID.
         * Location: 0x0A (2 bytes)
         * @return Item ID (0 = no item)
         */
        uint16_t heldItem() const noexcept override
        {
            return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x0A));
        }

        /** Sets the held item id (0x0A). The field exists in PB7; Let's Go doesn't use held items
         *  in-game, so a non-zero value here is cosmetic/illegal but harmless to the save. */
        void setHeldItem(uint16_t item) noexcept override
        {
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x0A), item);
            refreshChecksum();
        }

        /**
         * Gets the Pokemon's form/variation.
         * Location: 0x1D (1 byte)
         * @return Form ID (0 = base form)
         */
        uint8_t form() const noexcept override
        {
            // Byte 0x1D packs Fateful(bit0) + Gender(bits1-2) + Form(bits3-7).
            return static_cast<uint8_t>(data[0x1D]) >> 3;
        }

        /**
         * Gets the original trainer ID (32-bit format).
         * Location: 0x0C (4 bytes)
         * @return Trainer ID32 value
         */
        uint32_t id32() const noexcept override
        {
            return readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x0C));
        }

        /**
         * Gets the Pokemon's current experience points.
         * Location: 0x10 (4 bytes)
         * @return Experience value (determines level)
         */
        uint32_t exp() const noexcept override
        {
            return readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x10));
        }

        /** Sets total EXP (0x10) directly; level() derives from it, so this also drives the level. */
        void setExp(uint32_t value) noexcept override
        {
            writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x10), value);
            recalculateStats();
            refreshChecksum();
        }

        /**
         * Gets the Pokemon's ability ID.
         * Location: 0x14 (1 byte — PB7 ability is a u8; 0x15 holds AbilityNumber + flags).
         * @return Ability ID
         */
        uint16_t ability() const noexcept override
        {
            return static_cast<uint8_t>(data[0x14]);
        }
        void setAbility(uint16_t value) noexcept override
        {
            data[0x14] = static_cast<std::byte>(value & 0xFF);
            refreshChecksum();
        }

        /** Ability slot (1 / 2 / H). Location: 0x15 bits 0-2. */
        uint8_t abilityNumber() const noexcept override { return static_cast<uint8_t>(data[0x15]) & 0x07; }
        void setAbilityNumber(uint8_t number) noexcept override
        {
            uint8_t b = (static_cast<uint8_t>(data[0x15]) & ~0x07) | (number & 0x07);
            data[0x15] = static_cast<std::byte>(b);
            refreshChecksum();
        }

        /**
         * Gets the Pokemon's nature.
         * Location: 0x1C (1 byte)
         * Nature affects stat growth (e.g., Adamant boosts Attack, lowers Sp. Attack)
         * @return Nature ID (0-24)
         */
        uint8_t nature() const noexcept override
        {
            return static_cast<uint8_t>(data[0x1C]);
        }

        /** Sets the nature (0x1C). In Let's Go this is also the stat nature, so recalc stats. */
        void setNature(uint8_t value) noexcept override
        {
            data[0x1C] = static_cast<std::byte>(value);
            recalculateStats();
            refreshChecksum();
        }

        /** Sets gender (0=Male, 1=Female, 2=Genderless). Location: 0x1D bits 1-2 (preserves
         *  Fateful bit0 + Form bits3-7). */
        void setGender(uint8_t value) noexcept override
        {
            uint8_t b = (static_cast<uint8_t>(data[0x1D]) & ~0x06) | ((value & 0x03) << 1);
            data[0x1D] = static_cast<std::byte>(b);
            refreshChecksum();
        }

        /**
         * Gets the nature used for stat calculation.
         * In Let's Go, this is the same as nature() (no separate stat nature).
         * @return Nature ID (0-24)
         */
        uint8_t statNature() const noexcept override
        {
            return nature();
        }

        // ========================================
        // Encryption and Identification
        // ========================================

        /**
         * Gets the Encryption Constant.
         * Location: 0x00 (4 bytes)
         * Used as the seed for encrypting/decrypting Pokemon data.
         * @return Encryption Constant value
         */
        uint32_t encryptionConstant() const noexcept override
        {
            return readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x00));
        }

        /**
         * Gets the Personality ID (PID).
         * Location: 0x18 (4 bytes). This is SEPARATE from the Encryption Constant (0x00);
         * the EC is only the crypto seed. Shininess is derived from the PID.
         * @return PID value
         */
        uint32_t pid() const noexcept override
        {
            return readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x18));
        }

        // ========================================
        // Block B (Attacks/Nickname)
        // ========================================

        /**
         * Gets the Pokemon's nickname (custom name set by trainer).
         * Location: 0x40 (26 bytes, UTF-16LE)
         * Maximum 12 characters including null terminator.
         * @return Nickname as UTF-16 string
         */
        std::u16string nickname() const override
        {
            const uint8_t* nicknameStart = reinterpret_cast<const uint8_t*>(data.data() + 0x40);
            return getString(nicknameStart, 26);
        }

        /** Sets the nickname (0x40, 26 bytes / 12 chars). Does not change the isNicknamed flag. Was
         *  unwired (base no-op), so a created LGPE mon showed a BLANK name in-game. */
        void setNickname(const std::u16string& value) noexcept override
        {
            setString(reinterpret_cast<uint8_t*>(data.data() + 0x40), 26, value, 12);
            refreshChecksum();
        }
        /** "Has a custom nickname" flag -- bit 31 of the packed IV32 (0x74). */
        bool isNicknamed() const noexcept override { return (iv32() & 0x80000000u) != 0; }
        void setIsNicknamed(bool nicknamed) noexcept override
        {
            uint32_t iv = iv32();
            if (nicknamed) iv |= 0x80000000u; else iv &= ~0x80000000u;
            writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x74), iv);
            refreshChecksum();
        }

        // ========================================
        // Moves (Block B) — Let's Go layout (differs from Gen 8/9)
        // ========================================

        /** Move ID in a slot. Location: 0x5A + slot*2. */
        uint16_t move(int slot) const noexcept override
        {
            if (slot < 0 || slot > 3) return 0;
            return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x5A + slot * 2));
        }
        void setMove(int slot, uint16_t moveID) noexcept override
        {
            if (slot < 0 || slot > 3) return;
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x5A + slot * 2), moveID);
            refreshChecksum();
        }

        /** Current PP of a move slot. Location: 0x62 + slot. */
        uint8_t movePP(int slot) const noexcept override
        {
            if (slot < 0 || slot > 3) return 0;
            return static_cast<uint8_t>(data[0x62 + slot]);
        }
        void setMovePP(int slot, uint8_t pp) noexcept override
        {
            if (slot < 0 || slot > 3) return;
            data[0x62 + slot] = static_cast<std::byte>(pp);
            refreshChecksum();
        }

        /** PP Ups applied to a move slot. Location: 0x66 + slot. */
        uint8_t movePPUps(int slot) const noexcept override
        {
            if (slot < 0 || slot > 3) return 0;
            return static_cast<uint8_t>(data[0x66 + slot]);
        }
        void setMovePPUps(int slot, uint8_t ppUps) noexcept override
        {
            if (slot < 0 || slot > 3) return;
            data[0x66 + slot] = static_cast<std::byte>(ppUps);
            refreshChecksum();
        }

        /** Relearn move ID. Location: 0x6A + slot*2. */
        uint16_t relearnMove(int slot) const noexcept override
        {
            if (slot < 0 || slot > 3) return 0;
            return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x6A + slot * 2));
        }
        void setRelearnMove(int slot, uint16_t moveID) noexcept override
        {
            if (slot < 0 || slot > 3) return;
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x6A + slot * 2), moveID);
            refreshChecksum();
        }

        // ========================================
        // Block C/D (Misc)
        // ========================================

        /**
         * Gets the Pokemon's friendship/happiness value.
         * Location: 0xCA (1 byte)
         * In Let's Go, friendship affects stat calculations (10% bonus at max friendship).
         * @return Friendship value (0-255)
         */
        uint8_t friendship() const noexcept override
        {
            return static_cast<uint8_t>(data[0xCA]);
        }
        void setFriendship(uint8_t value) noexcept override { data[0xCA] = static_cast<std::byte>(value); refreshChecksum(); }

        // ---- OT / handler / origin / met / ball / language (PB7 offsets per PKHeX PB7.cs) ----
        /** Original Trainer name (0xB0, 26 bytes UTF-16). */
        std::u16string otName() const override { return getString(reinterpret_cast<const uint8_t*>(data.data() + 0xB0), 26); }
        void setOTName(const std::u16string& value) noexcept override { setString(reinterpret_cast<uint8_t*>(data.data() + 0xB0), 26, value, 12); refreshChecksum(); }
        /** Handling (current) Trainer name (0x78, 26 bytes). */
        std::u16string htName() const override { return getString(reinterpret_cast<const uint8_t*>(data.data() + 0x78), 26); }
        void setHTName(const std::u16string& value) noexcept override { setString(reinterpret_cast<uint8_t*>(data.data() + 0x78), 26, value, 12); refreshChecksum(); }
        /** Handling Trainer gender (0 = Male, 1 = Female). Location: 0x92 (PKHeX PB7.HandlingTrainerGender),
         *  directly before currentHandler below. This was the one HT field left on the base stub, and both
         *  halves of that stub were wrong: the getter returned a hard 0, so every
         *  handler displayed as Male whatever the save said, and the setter was a no-op, so a trainer gender
         *  change could never re-stamp it. The re-stamp still counted those mons as updated (the getter's 0
         *  never matched a Female trainer). */
        uint8_t htGender() const noexcept override { return static_cast<uint8_t>(data[0x92]); }
        void setHTGender(uint8_t value) noexcept override { data[0x92] = static_cast<std::byte>(value); refreshChecksum(); }
        /** Handling Trainer friendship (0-255). Location: 0xA2. Wired alongside htGender -- same omission,
         *  and an unwired getter that quietly returns 0 is what made the gender bug invisible. */
        uint8_t htFriendship() const noexcept override { return static_cast<uint8_t>(data[0xA2]); }
        void setHTFriendship(uint8_t value) noexcept override { data[0xA2] = static_cast<std::byte>(value); refreshChecksum(); }
        /** Current handler flag (0 = OT active, 1 = HT active). Location: 0x93. */
        uint8_t currentHandler() const noexcept override { return static_cast<uint8_t>(data[0x93]); }
        void setCurrentHandler(uint8_t value) noexcept override { data[0x93] = static_cast<std::byte>(value); refreshChecksum(); }
        /** Game of origin (Version byte). Location: 0xDF. */
        uint8_t originGame() const noexcept override { return static_cast<uint8_t>(data[0xDF]); }
        void setOriginGame(uint8_t value) noexcept override { data[0xDF] = static_cast<std::byte>(value); refreshChecksum(); }
        /** Language id. Location: 0xE3. */
        uint8_t language() const noexcept override { return static_cast<uint8_t>(data[0xE3]); }
        void setLanguage(uint8_t value) noexcept override { data[0xE3] = static_cast<std::byte>(value); refreshChecksum(); }
        /** Poke Ball id. Location: 0xDC. */
        uint8_t ball() const noexcept override { return static_cast<uint8_t>(data[0xDC]); }
        void setBall(uint8_t value) noexcept override { data[0xDC] = static_cast<std::byte>(value); refreshChecksum(); }
        /** Met location. Location: 0xDA (2 bytes). */
        uint16_t metLocation() const noexcept override { return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0xDA)); }
        void setMetLocation(uint16_t value) noexcept override { writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0xDA), value); refreshChecksum(); }
        /** Egg location. Location: 0xD8 (2 bytes). */
        uint16_t eggLocation() const noexcept override { return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0xD8)); }
        void setEggLocation(uint16_t value) noexcept override { writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0xD8), value); refreshChecksum(); }

        /** Met date (0xD4-0xD6) and received-Egg date (0xD1-0xD3); the year byte is years-since-2000.
         *  These were unwired (base no-ops), so a created LGPE mon's met date stayed 0/0/2000. */
        uint8_t metYear()  const noexcept override { return static_cast<uint8_t>(data[0xD4]); }
        void setMetYear(uint8_t value)  noexcept override { data[0xD4] = static_cast<std::byte>(value); refreshChecksum(); }
        uint8_t metMonth() const noexcept override { return static_cast<uint8_t>(data[0xD5]); }
        void setMetMonth(uint8_t value) noexcept override { data[0xD5] = static_cast<std::byte>(value); refreshChecksum(); }
        uint8_t metDay()   const noexcept override { return static_cast<uint8_t>(data[0xD6]); }
        void setMetDay(uint8_t value)   noexcept override { data[0xD6] = static_cast<std::byte>(value); refreshChecksum(); }
        uint8_t eggYear()  const noexcept override { return static_cast<uint8_t>(data[0xD1]); }
        void setEggYear(uint8_t value)  noexcept override { data[0xD1] = static_cast<std::byte>(value); refreshChecksum(); }
        uint8_t eggMonth() const noexcept override { return static_cast<uint8_t>(data[0xD2]); }
        void setEggMonth(uint8_t value) noexcept override { data[0xD2] = static_cast<std::byte>(value); refreshChecksum(); }
        uint8_t eggDay()   const noexcept override { return static_cast<uint8_t>(data[0xD3]); }
        void setEggDay(uint8_t value)   noexcept override { data[0xD3] = static_cast<std::byte>(value); refreshChecksum(); }
        /** Met level (bits 0-6). Location: 0xDD. */
        uint8_t metLevel() const noexcept override { return static_cast<uint8_t>(data[0xDD]) & 0x7F; }
        void setMetLevel(uint8_t value) noexcept override { uint8_t b = (static_cast<uint8_t>(data[0xDD]) & 0x80) | (value & 0x7F); data[0xDD] = static_cast<std::byte>(b); refreshChecksum(); }
        /** OT gender (bit 7 of 0xDD). */
        uint8_t otGender() const noexcept override { return (static_cast<uint8_t>(data[0xDD]) >> 7) & 0x01; }
        void setOTGender(uint8_t value) noexcept override { uint8_t b = (static_cast<uint8_t>(data[0xDD]) & 0x7F) | ((value & 0x01) << 7); data[0xDD] = static_cast<std::byte>(b); refreshChecksum(); }

        /**
         * Checks if the Pokemon is an egg.
         * Egg status is stored in a flag byte.
         * @return true if egg, false otherwise
         */
        bool isEgg() const noexcept override
        {
            return false; // Let's Go doesn't have eggs
        }

        /**
         * Checks if the Pokemon is infected with Pokerus.
         * Let's Go does not have the Pokerus mechanic.
         * @return false (always, Pokerus doesn't exist in Let's Go)
         */
        bool isPokerusInfected() const noexcept override
        {
            return false; // Let's Go doesn't have Pokerus
        }

        /**
         * Checks if the Pokemon has been cured of Pokerus.
         * Let's Go does not have the Pokerus mechanic.
         * @return false (always, Pokerus doesn't exist in Let's Go)
         */
        bool isPokerusCured() const noexcept override
        {
            return false; // Let's Go doesn't have Pokerus
        }

        // ========================================
        // Shiny and Gender
        // ========================================

        /**
         * Checks if the Pokemon is shiny (alternate coloration).
         * Let's Go uses the same shiny calculation as Gen 8.
         * @param trainerID32 The trainer's ID32 value
         * @param species Species name (for logging/debugging)
         * @return true if shiny, false otherwise
         */
        bool isShiny(uint32_t trainerID32, std::string species) const noexcept override
        {
            if (trainerID32 == 0) {
                return false;
            }
            // Shininess is PID-based (Gen 6+): shiny iff (TID^SID^PIDhi^PIDlo) < 16.
            uint32_t p = pid();
            uint32_t xorComponent = (p ^ trainerID32);
            uint32_t xorResult = (xorComponent ^ (xorComponent >> 16)) & 0xFFFF;
            return xorResult < 16;
        }

        /**
         * Gets the Pokemon's gender.
         * @return 0 = Male, 1 = Female, 2 = Genderless
         */
        uint8_t gender() const noexcept override;

        /**
         * Gets a gender symbol string for display.
         * @return "♂" for male, "♀" for female, "" for genderless
         */
        const char* genderSymbol() const noexcept override
        {
            uint8_t genderValue = gender();
            if (genderValue == 0) return "♂"; // Male
            if (genderValue == 1) return "♀"; // Female
            return ""; // Genderless
        }

        // ========================================
        // Stats - Effort Values (EVs) -- PRESENT IN THE FORMAT, UNUSED BY THE GAME
        // ========================================

        /**
         * Let's Go has NO EV mechanic. It replaced EV training with Awakening Values (below), and
         * nothing in the game reads these bytes: PB7's stat formula is AV + IV + base + level, with no
         * EV term at all (PKHeX `PB7.LoadStats`, mirrored by computeStat()).
         *
         * The bytes are nonetheless real. PB7 inherits the Gen 7 layout, so 0x1E-0x23 exist and PKHeX
         * maps `EV_HP`..`EV_SPD` onto exactly these offsets. On a legitimate Let's Go Pokemon they are
         * always 0 -- the game never writes them, and the bank's converter deliberately leaves them 0
         * when a Pokemon enters LGPE. A non-zero value here therefore means the Pokemon was edited by
         * something else, which is worth being able to SEE. That is the only reason these getters
         * exist: the legality checker reads EVs for every format, and reporting the real bytes beats
         * reporting a hardcoded 0 that would hide the anomaly.
         *
         * Overriding is not optional either way -- the base declares them pure virtual.
         */
        uint8_t evHP() const noexcept override  { return static_cast<uint8_t>(data[0x1E]); }
        uint8_t evATK() const noexcept override { return static_cast<uint8_t>(data[0x1F]); }
        uint8_t evDEF() const noexcept override { return static_cast<uint8_t>(data[0x20]); }
        uint8_t evSPE() const noexcept override { return static_cast<uint8_t>(data[0x21]); }
        uint8_t evSPA() const noexcept override { return static_cast<uint8_t>(data[0x22]); }
        uint8_t evSPD() const noexcept override { return static_cast<uint8_t>(data[0x23]); }

        /**
         * Deliberately does nothing. Writing an EV here would change no stat the game computes, while
         * making the Pokemon read as edited to anything that checks -- all cost, no effect. Use setAV().
         *
         * Nothing calls this for Let's Go today: every editor path branches on hasAwakeningValues() and
         * routes to setAV(). This is the backstop for the one that eventually forgets.
         */
        void setEV(int, uint8_t) noexcept override {}

        // ========================================
        // Stats - Awakening Values (AVs) - Let's Go Unique
        // ========================================

        /**
         * Awakening Values (AVs) - Unique to Pokemon Let's Go!
         * Location: 0x24-0x29 (1 byte each)
         * These provide stat bonuses similar to EVs but earned differently.
         * Max 200 per stat, earned by catching Pokemon of the same species.
         * Each AV point directly adds to the stat (different from EV formula).
         */
        uint8_t avHP() const noexcept override  { return static_cast<uint8_t>(data[0x24]); }
        uint8_t avATK() const noexcept override { return static_cast<uint8_t>(data[0x25]); }
        uint8_t avDEF() const noexcept override { return static_cast<uint8_t>(data[0x26]); }
        uint8_t avSPE() const noexcept override { return static_cast<uint8_t>(data[0x27]); }
        uint8_t avSPA() const noexcept override { return static_cast<uint8_t>(data[0x28]); }
        uint8_t avSPD() const noexcept override { return static_cast<uint8_t>(data[0x29]); }

        /**
         * Checks if this Pokemon format uses Awakening Values.
         * @return true for Let's Go Pokemon
         */
        bool hasAwakeningValues() const noexcept override { return true; }

        /**
         * Sets an Awakening Value for a specific stat.
         * @param statIndex 0=HP, 1=ATK, 2=DEF, 3=SPE, 4=SPA, 5=SPD
         * @param value AV value (0-200)
         */
        void setAV(int statIndex, uint8_t value) noexcept override {
            if (statIndex >= 0 && statIndex < 6 && value <= 200) {
                data[0x24 + statIndex] = static_cast<std::byte>(value);
                recalculateStats();
                refreshChecksum();
            }
        }

        // ========================================
        // Stats - Individual Values (IVs)
        // ========================================

        /**
         * Gets the packed IV32 value.
         * Location: 0x74 (4 bytes) - Different location from PK8!
         * Contains all 6 IVs plus special flags.
         */
        uint32_t iv32() const noexcept { return readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x74)); }

        /**
         * Individual Values (IVs) - inherent stat potential (0-31).
         * Same bit layout as PK8.
         */
        uint8_t ivHP() const noexcept override  { return (iv32() >> 0) & 0x1F; }
        uint8_t ivATK() const noexcept override { return (iv32() >> 5) & 0x1F; }
        uint8_t ivDEF() const noexcept override { return (iv32() >> 10) & 0x1F; }
        uint8_t ivSPE() const noexcept override { return (iv32() >> 15) & 0x1F; }
        uint8_t ivSPA() const noexcept override { return (iv32() >> 20) & 0x1F; }
        uint8_t ivSPD() const noexcept override { return (iv32() >> 25) & 0x1F; }

        /**
         * Sets an Individual Value for a specific stat.
         * @param statIndex 0=HP, 1=ATK, 2=DEF, 3=SPE, 4=SPA, 5=SPD
         * @param value IV value (0-31)
         */
        void setIV(int statIndex, uint8_t value) noexcept override {
            if (statIndex >= 0 && statIndex < 6 && value <= 31) {
                uint32_t iv = iv32();
                int shift = statIndex * 5;
                uint32_t mask = ~(0x1F << shift);
                iv = (iv & mask) | ((value & 0x1F) << shift);
                writeUInt32LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x74), iv);
                recalculateStats();
                refreshChecksum();
            }
        }

        // ========================================
        // Checksum Validation
        // ========================================

        /**
         * Gets the stored checksum value.
         * Location: 0x06 (2 bytes)
         * @return Checksum value
         */
        uint16_t checksum() const noexcept override {
            return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x06));
        }

        /**
         * Calculates the checksum from offset 0x08 to SIZE_6STORED.
         * @return Calculated checksum value
         */
        uint16_t calculateChecksum() const noexcept override {
            uint16_t checksum = 0;

            // Sum all 16-bit values from offset 0x08 to SIZE_6STORED
            const size_t checksumEnd = std::min(dataSize, SIZE_STORED7_LGPE);
            for (size_t i = 0x08; i < checksumEnd; i += 2) {
                checksum += readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + i));
            }

            return checksum;
        }

        /**
         * Recalculates and updates the stored checksum.
         * MUST be called after any modification to Pokemon data.
         */
        void refreshChecksum() noexcept override {
            uint16_t newChecksum = calculateChecksum();
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0x06), newChecksum);
        }

        /**
         * Validates that stored checksum matches calculated checksum.
         * @return true if valid, false if data is corrupted
         */
        bool checksumValid() const noexcept override {
            return checksum() == calculateChecksum();
        }

        // ========================================
        // Base Stats (Species-Dependent)
        // ========================================

        uint8_t baseHP() const noexcept override;
        uint8_t baseATK() const noexcept override;
        uint8_t baseDEF() const noexcept override;
        uint8_t baseSPE() const noexcept override;
        uint8_t baseSPA() const noexcept override;
        uint8_t baseSPD() const noexcept override;

        // ========================================
        // Calculated Stats (Battle Stats)
        // ========================================

        /**
         * Gets the Pokemon's current level.
         * Let's Go stores level in a different location than modern games.
         * @return Level (1-100)
         */
        uint8_t level() const noexcept override;

        /**
         * Battle stats in Let's Go = friendship% * ( nature% * ((2*Base + IV) * Level/100 + 5) ) + AV,
         * with HP the usual (no nature / friendship). computeStat() is the ONE implementation; the six
         * getters and recalculateStats() (which writes the stored tail the game reads) both use it so the
         * display can't drift from the game. idx 0..5 = HP, Atk, Def, Spe, SpA, SpD.
         */
        uint16_t computeStat(int idx) const noexcept;
        uint16_t statHPMax() const noexcept override;
        uint16_t statATK() const noexcept override;
        uint16_t statDEF() const noexcept override;
        uint16_t statSPE() const noexcept override;
        uint16_t statSPA() const noexcept override;
        uint16_t statSPD() const noexcept override;
        uint16_t statHPCurrent() const noexcept override {
            return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0xF0));
        }
        void setStatHPCurrent(uint16_t value) noexcept override {
            const uint16_t max = statHPMax();
            if (max != 0 && value > max) value = max;
            writeUInt16LittleEndian(reinterpret_cast<uint8_t*>(data.data() + 0xF0), value);
        }


        // ========================================
        // Stat Recalculation
        // ========================================

        /**
         * Recalculates all battle stats including AVs and friendship bonuses.
         * Let's Go formula:
         * Stat = (((2 * Base + IV + EV/4) * Level / 100) + 5) * Nature * Friendship + AV
         *
         * Where Friendship is a multiplier: (friendship/255 / 10 + 1) ≈ 1.0 to 1.1
         */
        void recalculateStats() noexcept override;

        // ========================================
        // Advanced Modification
        // ========================================

        /**
         * Regenerates encryption constant while maintaining gender and shininess.
         * Let's Go uses EC as PID, so this modifies the EC.
         * Used to fix legality issues when IVs are modified.
         * @param trainerID32 The trainer's ID32 for shiny calculation
         */
        void regeneratePID(uint32_t trainerID32) noexcept override;

        /**
         * Sets the shiny status of the Pokemon.
         * Modifies encryption constant while preserving gender.
         * @param makeShiny true to make shiny, false to make non-shiny
         * @param trainerID32 The trainer's ID32 for shiny calculation
         */
        void setShiny(bool makeShiny, uint32_t trainerID32) noexcept override;

        // ========================================
        // Let's Go Unique Features
        // ========================================

        /**
         * Gets the Combat Power (CP) value.
         * Location: 0xFE (2 bytes)
         * CP is similar to Pokemon GO's power rating.
         * @return CP value (0-10000)
         */
        uint16_t cp() const noexcept {
            return readUInt16LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0xFE));
        }

        /**
         * Gets the height scalar (individual size variation).
         * Location: 0x3A (1 byte)
         * Used to calculate actual height.
         * @return Height scalar (0-255)
         */
        uint8_t heightScalar() const noexcept {
            return static_cast<uint8_t>(data[0x3A]);
        }

        /**
         * Gets the weight scalar (individual size variation).
         * Location: 0x3B (1 byte)
         * Used to calculate actual weight.
         * @return Weight scalar (0-255)
         */
        uint8_t weightScalar() const noexcept {
            return static_cast<uint8_t>(data[0x3B]);
        }

        /**
         * Gets the absolute height in meters.
         * Location: 0x2C (4 bytes, float)
         * @return Height in meters
         */
        float heightAbsolute() const noexcept {
            uint32_t bits = readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0x2C));
            float result;
            std::memcpy(&result, &bits, sizeof(float));
            return result;
        }

        /**
         * Gets the absolute weight in kilograms.
         * Location: 0xE4 (4 bytes, float)
         * @return Weight in kilograms
         */
        float weightAbsolute() const noexcept {
            uint32_t bits = readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(data.data() + 0xE4));
            float result;
            std::memcpy(&result, &bits, sizeof(float));
            return result;
        }
    };
}

#endif
