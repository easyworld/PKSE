/**
 * Trainer8SWSH.h - Generation 8 Trainer/Save File Data Management
 *
 * This file defines the Trainer8SWSH class for Pokemon Generation 8 games:
 * - Pokemon Sword/Shield
 *
 * Trainer8SWSH implements generation-specific logic for:
 * - PK8 Pokemon storage (party and boxes)
 * - Gen 8 block keys
 * - Gen 8 encryption (encryptArray8/decryptArray8)
 * - Gen 8-specific save file structure
 */

#ifndef TRAINER_TRAINER8_SWSH_H
#define TRAINER_TRAINER8_SWSH_H

#include <cstring>

#include "Trainer/Trainer.h"
#include "Trainer/Inventory8SWSH.h"
#include "Pokemon/Pokemon8SWSH.h"
#include "Encryption/Encryption8SWSH.h"
#include "Save/Block.h"

namespace Trainer {
    // ========================================
    // Generation 8 SWSH Save File Block Keys
    // ========================================
    // Block keys are consistent across all game versions (1.0 through 1.3.2)
    // DLC content is detected by the presence of additional blocks (R1, R2)
    // rather than changes to existing block keys or offsets.
    //
    // Version detection:
    // - Base game (v1.0-1.1): Only base blocks present
    // - Isle of Armor (v1.2+): SAVE_REVISION8_R1_SWSH block present
    // - Crown Tundra (v1.3+): SAVE_REVISION8_R2_SWSH block present
    // ========================================

    // Core game blocks (present in all versions)
    constexpr size_t MY_STATUS8_SWSH = 0xf25c070e;          // Trainer Details
    constexpr size_t PARTY8_SWSH = 0x2985fe5d;              // Party Data
    constexpr size_t MONEY8_SWSH = 0x1b882b09;              // Money/Misc
    constexpr size_t TRAINER_CARD8_SWSH = 0x874da6fa;       // Trainer Card
    constexpr size_t PLAY_TIME8_SWSH = 0x8cbbfd90;          // Time Played
    constexpr size_t ITEM8_SWSH = 0x1177c2c4;               // Items
    constexpr size_t BOX8_SWSH = 0x0d66012c;                // Box Data
    constexpr size_t BOX_LAYOUT8_SWSH = 0x19722c89;         // Box Names
    constexpr size_t CURRENT_BOX8_SWSH = 0x017C3CBB;        // Current box index ("U32 Box Index")
    // constexpr size_t BOX_WALLPAPERS8_SWSH = 0x2EB1B190;  // Box Wallpapers

    // DLC Detection Block Keys
    // These blocks are only present if the corresponding DLC has been played
    constexpr size_t SAVE_REVISION8_SWSH = 0x4716c404;              // Base game Pokedex (Galar)
    constexpr size_t SAVE_REVISION8_R1_SWSH = 0x3F936BA9;           // Isle of Armor Pokedex (DLC 1)
    constexpr size_t SAVE_REVISION8_R2_SWSH = 0x3C9366F0;           // Crown Tundra Pokedex (DLC 2)

    // DLC-specific blocks (for future use)
    // constexpr size_t RAID_SPAWN_LIST8_R1_SWSH = 0x158DA896;  // IoA Raid Data
    // constexpr size_t RAID_SPAWN_LIST8_R2_SWSH = 0x148DA703;  // CT Raid Data

    // Generation 8 constants
    constexpr size_t BOX_COUNT8_SWSH = 32;       // Number of boxes in Sword/Shield
    constexpr size_t BOX_NAME_LENGTH8_SWSH = 0x22; // 34 bytes per box name (UTF-16LE)
    // 34 bytes = 17 UTF-16 slots, one of which holds the null terminator (PKHeX: SAV6.LongStringLength / 2).
    constexpr size_t MAX_BOX_NAME_CHARS8_SWSH = BOX_NAME_LENGTH8_SWSH / 2 - 1;   // 16

    /**
     * Trainer8SWSH - Generation 8 SWSH Trainer Class
     *
     * Inherits from Trainer base class and implements Gen 8-specific save file format.
     * Handles automatic decryption on construction and provides Gen 8-specific
     * encryption when updating blocks.
     *
     * Gen 8 Specific Features:
     * - 32 boxes with 30 slots each
     * - PK8 Pokemon format (344 bytes party, 328 bytes stored)
     * - Block-based save file structure
     * - Gen 8 encryption algorithm
     *
     * Save File Structure (Sword/Shield):
     * - File: "main" (approximately 1.6MB)
     * - Format: Multiple blocks identified by key values
     * - Encryption: Block-level encryption + Pokemon encryption
     */
    class Trainer8SWSH final : public Trainer
    {
    public:
        // ========================================
        // Constructor
        // ========================================

        /**
         * Constructs a Trainer8SWSH object from save file blocks.
         *
         * Process:
         * 1. Parses blocks to extract trainer info
         * 2. Decrypts and loads party Pokemon (Pokemon8)
         * 3. Decrypts and loads box Pokemon (Pokemon8)
         * 4. Loads items and box names
         *
         * @param blocks Save file blocks parsed from Gen 8 save file
         */
        explicit Trainer8SWSH(std::vector<Save::Block> blocks) : Trainer(std::move(blocks))
        {
            party.reserve(MAX_PARTY_SLOTS);
            boxes.resize(BOX_COUNT8_SWSH);
            boxNames.resize(BOX_COUNT8_SWSH);

            // Parse all blocks to extract data
            for (const auto& block : this->blocks) {
                parseBlock(block);
            }

            // Detect DLC version by checking for DLC Pokedex blocks
            detectSaveRevision();
        }

        /// Destructor - cleanup handled by base class and unique_ptrs
        ~Trainer8SWSH() override = default;

        // Delete copy operations
        Trainer8SWSH(const Trainer8SWSH&) = delete;
        Trainer8SWSH& operator=(const Trainer8SWSH&) = delete;

        // Allow move operations
        Trainer8SWSH(Trainer8SWSH&&) noexcept = default;
        Trainer8SWSH& operator=(Trainer8SWSH&&) noexcept = default;

        // ========================================
        // Implementation of Pure Virtual Methods
        // ========================================

        /**
         * Updates the PARTY_KEY block with modified Pokemon data.
         * Uses Gen 8 encryption (encryptArray8).
         */
        void updatePartyBlock() override;

        /**
         * Updates the BOX_KEY block with modified Pokemon data.
         * Uses Gen 8 encryption (encryptArray8).
         */
        void updateBoxBlock() override;
        void updateBoxNameBlock() override;
        void updateCurrentBoxBlock() override;
        bool supportsBoxNames() const noexcept override { return true; }
        size_t getMaxBoxNameLength() const noexcept override { return MAX_BOX_NAME_CHARS8_SWSH; }

        /**
         * Updates the ITEM_KEY block with modified inventory data.
         */
        void updateItemBlock() override;

        /**
         * Creates a species-0, checksum-valid blank PK8 entity (mirrors updateBoxBlock()'s
         * encrypted-blank fallback: zeroed SIZE_PARTY8_SWSH buffer -> encryptArray8SWSH(seed 0)
         * -> Pokemon8SWSH). Starting point for the Pokemon creator.
         */
        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        /**
         * Gets the number of boxes available in Gen 8.
         * @return 32 (Sword/Shield has 32 boxes)
         */
        size_t getBoxCount() const noexcept override {
            return BOX_COUNT8_SWSH;
        }

        /**
         * Gets the number of slots per box in Gen 8.
         * @return 30 (Sword/Shield has 30 slots per box, 6x5 grid)
         */
        size_t getSlotsPerBox() const noexcept override {
            return BOX_SLOTS;
        }

        /**
         * Gets the number of Pokemon currently in the party.
         * @return Party size (0-6)
         */
        size_t getPartySize() const noexcept override {
            return party.size();
        }

        /**
         * Gets the game group for Gen 8 trainers.
         * @return GameVersion::SWSH (Sword/Shield group)
         */
        GameVersion getGameGroup() const noexcept override {
            return GameVersion::SWSH;
        }

    protected:

    private:
        /**
         * Parses a single block to extract relevant data.
         * Called during construction for each block in the save file.
         *
         * @param block The block to parse
         */
        void parseBlock(const Block& block);

        /**
         * Parses the MY_STATUS block to extract trainer ID.
         * Location: ID32 at offset 0xA0 (4 bytes)
         */
        void parseMyStatusBlock(const Block& block);

        /**
         * Parses the PARTY block to extract party Pokemon.
         * Format: 6 slots of SIZE_PARTY (344 bytes each)
         */
        void parsePartyBlock(const Block& block);

        /**
         * Parses the MISC block to extract money.
         * Location: Money at offset 0x04 (4 bytes)
         */
        void parseMoneyBlock(const Block& block);

        /**
         * Parses the TRAINER_CARD block to extract trainer name.
         * Location: Name at offset 0x00 (26 bytes, UTF-16LE)
         * Location: Trainer ID at offset 0x1C (4 bytes)
         */
        void parseTrainerCardBlock(const Block& block);

        /**
         * Parses the ITEM block to extract inventory items.
         * Format: Multiple pouches with variable item counts
         */
        void parseItemBlock(const Block& block);

        /**
         * Parses the BOX block to extract box Pokemon.
         * Format: 32 boxes * 30 slots * SIZE_PARTY (344 bytes)
         */
        void parseBoxBlock(const Block& block);

        /**
         * Parses the BOX_LAYOUT block to extract box names.
         * Format: 32 names * BOX_NAME_LENGTH (34 bytes, UTF-16LE)
         */
        void parseBoxLayoutBlock(const Block& block);

        /** Parses the CURRENT_BOX block ("U32 Box Index") so the editor opens on the last box used. */
        void parseCurrentBoxBlock(const Block& block);

        /**
         * Detects the save revision (DLC version) by checking for DLC Pokedex blocks.
         * Revision values:
         * - 0: Base game
         * - 1: Isle of Armor (IoA)
         * - 2: Crown Tundra (CT)
         */
        void detectSaveRevision();
    };
}

#endif
