/**
 * Generation 9 Legends: Z-A Trainer/Save File Data Management
 *
 * This file defines the Trainer9LZA class for Pokemon Generation 9 games:
 * - Pokemon Legends: Z-A (ZA)
 *
 * Trainer9LZA implements generation-specific logic for:
 * - PK9 Pokemon storage (party and boxes)
 * - Gen 9 block keys
 * - Gen 9 encryption (encryptArray9/decryptArray9)
 * - Gen 9-specific save file structure
 */

#ifndef TRAINER_TRAINER9_H
#define TRAINER_TRAINER9_H

#include <cstring>

#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon9LZA.h"
#include "Encryption/Encryption9LZA.h"

using namespace Pokemon;
using namespace Encryption;

namespace Trainer {
    // ========================================
    // Generation 9 Legends: Z-A Save File Block Keys
    // ========================================
    // Block keys are consistent across all game versions.
    // DLC/version content is tracked via the SAVE_REVISION block.
    //
    // Save Revision values (KSaveRevision):
    // - 0: Base game (v1.0.x)
    // - 1: Mega Dimension DLC (v2.0.0+)
    //
    // The save revision determines:
    // - Which Pokemon species are available
    // - Which moves are legal
    // - Which items exist
    // ========================================

    // Core game blocks (present in all versions)
    constexpr size_t MY_STATUS9_LZA = 0xE3E89BD1;           // Trainer Details
    constexpr size_t PARTY9_LZA = 0x3AA1A9AD;               // Party Data
    constexpr size_t MONEY9_LZA = 0x4F35D0DD;               // Money (u32)
    constexpr size_t PLAY_TIME9_LZA = 0xEDAFF794;           // Time Played
    constexpr size_t ITEM9_LZA = 0x21C9BD44;                // Items
    constexpr size_t BOX9_LZA = 0x0d66012c;                 // Box Data
    constexpr size_t BOX_LAYOUT9_LZA = 0x19722c89;          // Box Names
    constexpr size_t CURRENT_BOX9_LZA = 0x017C3CBB;         // Current box index ("U32 Box Index")
    // constexpr size_t BOX_WALLPAPERS9_LZA = 0x2EB1B190;   // Box Wallpapers

    // Version detection block
    constexpr size_t SAVE_REVISION9_LZA = 0x0926555A;       // Save Revision (u64)

    // Additional blocks (for future use)
    // Pokedex. ONE block, unlike Scarlet/Violet's pair -- Z-A has a single regional dex. Entries are
    // 0x84 bytes, keyed by the Gen 9 INTERNAL species id (PKHeX Zukan9a / PokeDexEntry9a).
    constexpr size_t POKEDEX9_LZA = 0x2D87BE5C;
    // constexpr size_t LAST_SAVED9_LZA = 0x1522C79C;       // Last save timestamp
    // constexpr size_t EVENT_FLAG9_LZA = 0x58505C5E;       // Game event flags

    // Generation 9 constants
    constexpr size_t BOX_COUNT9_LZA = 32; // Number of boxes in Legends: Z-A
    constexpr size_t BOX_NAME_LENGTH9_LZA = 0x22; // 34 bytes per box name (UTF-16LE)

    /**
     * Trainer9LZA - Generation 9 Trainer Class
     *
     * Inherits from Trainer base class and implements Gen 9-specific save file format.
     * Handles automatic decryption on construction and provides Gen 9-specific
     * encryption when updating blocks.
     *
     * Gen 9 Specific Features:
     * - 32 boxes with 30 slots each
     * - PK9 Pokemon format (344 bytes party, 328 bytes stored)
     * - Block-based save file structure
     * - Gen 9 encryption algorithm
     *
     * Save File Structure (Legends: Z-A):
     * - File: "main" (approximately 2.94MB)
     * - Format: Multiple blocks identified by key values
     * - Encryption: Block-level encryption + Pokemon encryption
     */
    class Trainer9LZA final : public Trainer
    {
    public:
        // ========================================
        // Constructor
        // ========================================

        /**
         * Constructs a Trainer9LZA object from save file blocks.
         *
         * Process:
         * 1. Parses blocks to extract trainer info
         * 2. Decrypts and loads party Pokemon (PK9)
         * 3. Decrypts and loads box Pokemon (PK9)
         * 4. Loads items and box names
         *
         * @param blocks Save file blocks parsed from Gen 9 save file
         */
        // Legends: Z-A (PA9). Shares the Gen 9 SCBlock save + entity format with Scarlet/Violet, but
        // Z-A GAPS its box/party slots (see the slot-stride members below) where S/V packs them.
        // Trainer9SV is the Scarlet/Violet counterpart.
        explicit Trainer9LZA(std::vector<Block> blocks) : Trainer(std::move(blocks))
        {
            party.reserve(MAX_PARTY_SLOTS);
            boxes.resize(BOX_COUNT9_LZA);
            boxNames.resize(BOX_COUNT9_LZA);

            // Parse all blocks to extract data (includes SAVE_REVISION9_LZA -> parseSaveRevisionBlock).
            for (const auto& block : this->blocks) {
                parseBlock(block);
            }
        }

        /// Destructor - cleanup handled by base class and unique_ptrs
        ~Trainer9LZA() override = default;

        // Delete copy operations
        Trainer9LZA(const Trainer9LZA&) = delete;
        Trainer9LZA& operator=(const Trainer9LZA&) = delete;

        // Allow move operations
        Trainer9LZA(Trainer9LZA&&) noexcept = default;
        Trainer9LZA& operator=(Trainer9LZA&&) noexcept = default;

        // ========================================
        // Implementation of Pure Virtual Methods
        // ========================================

        /**
         * Updates the PARTY_KEY block with modified Pokemon data.
         * Uses Gen 9 encryption (encryptArray9).
         */
        void updatePartyBlock() override;

        /**
         * Updates the BOX_KEY block with modified Pokemon data.
         * Uses Gen 9 encryption (encryptArray9).
         */
        void updateBoxBlock() override;
        void updateBoxNameBlock() override;
        void updateCurrentBoxBlock() override;
        bool supportsBoxNames() const noexcept override { return true; }
        size_t getMaxBoxNameLength() const noexcept override { return BOX_NAME_LENGTH9_LZA / 2 - 1; }

        /**
         * Updates the ITEM_KEY block with modified inventory data.
         */
        void updateItemBlock() override;
        void updateTrainerInfoBlock() override;   // money / OT name
        void updatePokedexBlock() override;       // Zukan9a: seen/caught/shiny/mega/alpha per form
        bool itemsAreIdIndexed() const override { return true; }   // count at itemId * 0x10

        /**
         * Creates a species-0, checksum-valid blank PK9 entity (mirrors updateBoxBlock()'s
         * encrypted-blank fallback: zeroed SIZE_PARTY9_LZA buffer -> encryptArray9LZA(seed 0)
         * -> Pokemon9LZA). Starting point for the Pokemon creator.
         */
        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        /**
         * Gets the number of boxes available in Gen 9.
         * @return 32 (Legends: Z-A has 32 boxes)
         */
        size_t getBoxCount() const noexcept override {
            return BOX_COUNT9_LZA;
        }

        /**
         * Gets the number of slots per box in Gen 9.
         * @return 30 (Legends: Z-A has 30 slots per box, 6x5 grid)
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
         * Gets the game group for Gen 9 trainers.
         * @return GameVersion::SWSH (Legends: Z-A group)
         */
        GameVersion getGameGroup() const noexcept override {
            return GameVersion::ZA;
        }

    private:
        // Legends: Z-A GAPS its slots: each box/party slot holds SIZE_PARTY9_LZA bytes of mon data
        // followed by a GAP_BOX_SLOT9_LZA-byte gap. (Trainer9SV packs its slots — no gap.)
        size_t m_partySlotStride = PARTY_SLOT_SIZE9_LZA;  // stride between party slots (gapped)
        size_t m_boxSlotStride   = BOX_SLOT_SIZE9_LZA;    // stride between box slots (gapped)
        size_t m_slotGapZero     = GAP_BOX_SLOT9_LZA;     // gap bytes zeroed after mon data on write
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
         * Format: 6 slots of SIZE_8PARTY (344 bytes each)
         */
        void parsePartyBlock(const Block& block);

        /**
         * Parses the MISC block to extract money.
         * Location: Money at offset 0x04 (4 bytes)
         */
        void parseMoneyBlock(const Block& block);

        // THERE'S NO TRAINER CARD PARSING IN GEN 9

        /**
         * Parses the ITEM block to extract inventory items.
         * Format: Multiple pouches with variable item counts
         */
        void parseItemBlock(const Block& block);

        /**
         * Parses the BOX block to extract box Pokemon.
         * Format: 32 boxes * 30 slots * SIZE_9PARTY (344 bytes)
         */
        void parseBoxBlock(const Block& block);

        /**
         * Parses the BOX_LAYOUT block to extract box names.
         * Format: 32 names * BOX_NAME_LENGTH (34 bytes, UTF-16LE)
         */
        void parseBoxLayoutBlock(const Block& block);

        /**
         * Parses the CURRENT_BOX block ("U32 Box Index") to extract which box the game was last
         * left on, so the editor opens on the same box.
         */
        void parseCurrentBoxBlock(const Block& block);

        /**
         * Parses the SAVE_REVISION block to detect DLC version.
         * Revision values:
         * - 0: Base game
         * - 1: Mega Dimension (MD) DLC
         */
        void parseSaveRevisionBlock(const Block& block);
    };
}

#endif
