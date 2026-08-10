/**
 * Generation 9 Scarlet/Violet Trainer/Save File Data Management
 *
 * This file defines the Trainer9SV class for Pokemon Scarlet/Violet (SV).
 *
 * Trainer9SV implements Scarlet/Violet-specific logic for:
 * - PK9 Pokemon storage (party and boxes)
 * - Gen 9 block keys
 * - Gen 9 encryption (encryptArray9/decryptArray9)
 * - Gen 9-specific save file structure
 */

#ifndef TRAINER_TRAINER9SV_H
#define TRAINER_TRAINER9SV_H

#include <cstring>

#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon9SV.h"
#include "Encryption/Encryption9SV.h"

using namespace Pokemon;
using namespace Encryption;

namespace Trainer {
    // ========================================
    // Generation 9 Scarlet/Violet Save File Block Keys
    // ========================================
    // Block keys are consistent across all game versions.
    //
    // S/V has NO save-revision block. Unlike Legends: Z-A -- which does store one -- the DLC level
    // here is inferred from which blocks EXIST (PKHeX SAV9SV): the Blueberry-points block means
    // The Indigo Disk, and a DLC tera-raid block means The Teal Mask. See detectSaveRevision(),
    // the same approach Trainer8SWSH already uses for Isle of Armor / Crown Tundra.
    // ========================================

    // Core game blocks (present in all versions)
    constexpr size_t MY_STATUS9_SV = 0xE3E89BD1;           // Trainer Details
    constexpr size_t PARTY9_SV = 0x3AA1A9AD;               // Party Data
    constexpr size_t MONEY9_SV = 0x4F35D0DD;               // Money (u32)
    constexpr size_t PLAY_TIME9_SV = 0xEDAFF794;           // Time Played
    constexpr size_t ITEM9_SV = 0x21C9BD44;                // Items
    constexpr size_t BOX9_SV = 0x0d66012c;                 // Box Data
    constexpr size_t BOX_LAYOUT9_SV = 0x19722c89;          // Box Names
    constexpr size_t CURRENT_BOX9_SV = 0x017C3CBB;         // Current box index ("U32 Box Index")
    // constexpr size_t BOX_WALLPAPERS9_SV = 0x2EB1B190;   // Box Wallpapers

    // DLC-presence markers, read only by detectSaveRevision() (never written).
    constexpr size_t BLUEBERRY_POINTS9_SV = 0x66A33824;    // u32 BP -- exists only with The Indigo Disk
    constexpr size_t TERA_RAID_DLC9_SV    = 0x100B93DA;    // Kitakami + Blueberry raid dens (2 x 0xC80)

    // Additional blocks (for future use)
    // Pokedex. TWO blocks: the original Paldea one and the Kitakami one added by the DLC. From game
    // version 2.0.1 the developers dummied out the Paldea block and use Kitakami exclusively, so the
    // rule is "if the Kitakami block has data, it IS the dex" (PKHeX Zukan9). Their entry layouts
    // differ (0x18 vs 0x20), so which one is live decides how an entry is written.
    constexpr size_t ZUKAN9_SV_PALDEA   = 0x0DEAAEBD;
    constexpr size_t ZUKAN9_SV_KITAKAMI = 0xF5D7C0E2;
    // constexpr size_t LAST_SAVED9_SV = 0x1522C79C;       // Last save timestamp
    // constexpr size_t EVENT_FLAG9_SV = 0x58505C5E;       // Game event flags

    // Generation 9 constants
    constexpr size_t BOX_COUNT9_SV = 32; // Number of boxes in Scarlet/Violet
    constexpr size_t BOX_NAME_LENGTH9_SV = 0x22; // 34 bytes per box name (UTF-16LE)

    /**
     * Trainer9SV - Generation 9 Trainer Class
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
     * Save File Structure (Scarlet/Violet):
     * - File: "main" (approximately 2.94MB)
     * - Format: Multiple blocks identified by key values
     * - Encryption: Block-level encryption + Pokemon encryption
     */
    class Trainer9SV final : public Trainer
    {
    public:
        // ========================================
        // Constructor
        // ========================================

        /**
         * Constructs a Trainer9SV object from save file blocks.
         *
         * Process:
         * 1. Parses blocks to extract trainer info
         * 2. Decrypts and loads party Pokemon (PK9)
         * 3. Decrypts and loads box Pokemon (PK9)
         * 4. Loads items and box names
         *
         * @param blocks Save file blocks parsed from Gen 9 save file
         */
        // Scarlet/Violet (PK9). Shares the Gen 9 SCBlock save + entity format with Legends: Z-A, but
        // S/V PACKS its box/party slots (no inter-slot gap) where Z-A gaps them — see the slot-stride
        // members below. Trainer9LZA is the Z-A counterpart.
        explicit Trainer9SV(std::vector<Block> blocks) : Trainer(std::move(blocks))
        {
            party.reserve(MAX_PARTY_SLOTS);
            boxes.resize(BOX_COUNT9_SV);
            boxNames.resize(BOX_COUNT9_SV);

            for (const auto& block : this->blocks) {
                parseBlock(block);
            }
            detectSaveRevision();   // by block presence -- S/V stores no revision value
        }

        /// Destructor - cleanup handled by base class and unique_ptrs
        ~Trainer9SV() override = default;

        // Delete copy operations
        Trainer9SV(const Trainer9SV&) = delete;
        Trainer9SV& operator=(const Trainer9SV&) = delete;

        // Allow move operations
        Trainer9SV(Trainer9SV&&) noexcept = default;
        Trainer9SV& operator=(Trainer9SV&&) noexcept = default;

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
        size_t getMaxBoxNameLength() const noexcept override { return BOX_NAME_LENGTH9_SV / 2 - 1; }

        /**
         * Updates the ITEM_KEY block with modified inventory data.
         */
        void updateItemBlock() override;
        void updateTrainerInfoBlock() override;
        void updatePokedexBlock() override;      // Paldea / Kitakami Zukan blocks
        bool itemsAreIdIndexed() const override { return true; }   // count at itemId * 0x10

        /**
         * Creates a species-0, checksum-valid blank PK9 entity (mirrors updateBoxBlock()'s
         * encrypted-blank fallback: zeroed SIZE_PARTY9_SV buffer -> encryptArray9SV(seed 0)
         * -> Pokemon9SV). Starting point for the Pokemon creator.
         */
        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        /**
         * Gets the number of boxes available in Gen 9.
         * @return 32 (Scarlet/Violet has 32 boxes)
         */
        size_t getBoxCount() const noexcept override {
            return BOX_COUNT9_SV;
        }

        /**
         * Gets the number of slots per box in Gen 9.
         * @return 30 (Scarlet/Violet has 30 slots per box, 6x5 grid)
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
         * Gets the game group for Scarlet/Violet trainers.
         * @return GameVersion::SV
         */
        GameVersion getGameGroup() const noexcept override {
            return GameVersion::SV;
        }

    private:
        // Scarlet/Violet PACKS its slots: box and party slots are exactly SIZE_PARTY9_SV bytes with no
        // inter-slot gap. (Named members mirror Trainer9LZA — which gaps its slots — so the shared
        // parse/serialize logic in the .cpp reads identically across the two classes.)
        size_t m_partySlotStride = SIZE_PARTY9_SV;  // packed: no gap between party slots
        size_t m_boxSlotStride   = SIZE_PARTY9_SV;  // packed: no gap between box slots
        size_t m_slotGapZero     = 0;               // no gap bytes after mon data
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
         * Parses the CURRENT_BOX block ("U32 Box Index") so the editor opens on the box the game
         * was last left on.
         */
        void parseCurrentBoxBlock(const Block& block);

        /**
         * Detects the DLC level from which blocks the save contains -- S/V has no revision value
         * to read. Revision values:
         * - 0: Base game
         * - 1: The Teal Mask (TM)
         * - 2: The Indigo Disk (ID)
         */
        void detectSaveRevision();
    };
}

#endif
