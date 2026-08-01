/**
 * Generation 8 Legends: Arceus Trainer/Save File Data Management
 *
 * This file defines the Trainer8LA class for Pokemon Legends: Arceus (LA).
 *
 * Trainer8LA implements Legends: Arceus-specific logic for:
 * - PA8 Pokemon storage (party and boxes)
 * - Gen 8 block keys
 * - Gen 8 encryption (encryptArray8LA/decryptArray8LA)
 * - Gen 8-specific save file structure
 */

#ifndef TRAINER_TRAINER8LA_H
#define TRAINER_TRAINER8LA_H

#include <cstring>

#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon8LA.h"
#include "Encryption/Encryption8LA.h"
#include "Trainer/Inventory8LA.h"

using namespace Pokemon;
using namespace Encryption;

namespace Trainer {
    // ========================================
    // Legends: Arceus (SAV8LA) Save File Block Keys — SwishCrypto SCBlock keys (FNV hashes).
    // ========================================

    // Core game blocks
    constexpr size_t MY_STATUS8_LA = 0xf25c070e;           // Trainer Details (MyStatus8a)
    constexpr size_t PARTY8_LA = 0x2985fe5d;               // Party Data
    constexpr size_t MONEY8_LA = 0x3279D927;               // Money (u32 scalar block, max 9,999,999)
    constexpr size_t PLAY_TIME8_LA = 0xC4FA7C8C;           // Time Played
    constexpr size_t BOX8_LA = 0x47E1CEAB;                 // Box Data (stored-size 0x168 slots)
    constexpr size_t BOX_LAYOUT8_LA = 0x19722c89;          // Box Names

    // Item pouches — each is a packed list of 4-byte {itemId u16, count u16} entries (Inventory8LA.h).
    constexpr size_t ITEM_REGULAR8_LA = 0x9FE2790A;        // General items
    constexpr size_t ITEM_KEY8_LA     = 0x59A4D0C3;        // Key items
    constexpr size_t ITEM_STORED8_LA  = 0x8E434F0D;        // Item storage box
    constexpr size_t ITEM_RECIPE8_LA  = 0xF5D9F4A5;        // Crafting recipes
    constexpr size_t SATCHEL_UPGRADES8_LA = 0x75CE2CF6;    // u32 Satchel Upgrades (0-39): grows the bag

    // Fixed pouch capacities from PKHeX PlayerBag8a: Key Items 100, Storage 180, Recipes 70. The
    // general Items bag is min(675, SatchelUpgrades + 20) -- computed at runtime (see .cpp).
    constexpr size_t POUCH_CAP_KEY8_LA     = 100;
    constexpr size_t POUCH_CAP_STORED8_LA  = 180;
    constexpr size_t POUCH_CAP_RECIPE8_LA  = 70;
    constexpr size_t POUCH_CAP_REGULAR_MAX8_LA = 675;
    // constexpr size_t BOX_WALLPAPERS8_LA = 0x2EB1B190;   // Box Wallpapers

    // Version detection block
    constexpr size_t SAVE_REVISION8_LA = 0x0926555A;       // Save Revision (u64)

    // Additional blocks (for future use)
    // constexpr size_t POKEDEX8_LA = 0x2D87BE5C;          // Pokedex completion
    // constexpr size_t LAST_SAVED8_LA = 0x1522C79C;       // Last save timestamp
    // constexpr size_t EVENT_FLAG8_LA = 0x58505C5E;       // Game event flags
    constexpr size_t CURRENT_BOX8_LA = 0x017C3CBB;         // Current box index ("U8 Box Index")

    // Generation 8 constants
    constexpr size_t BOX_COUNT8_LA = 32; // Number of boxes in Legends: Arceus
    constexpr size_t BOX_NAME_LENGTH8_LA = 0x22; // 34 bytes per box name (UTF-16LE)

    /**
     * Trainer8LA - Generation 8 Trainer Class
     *
     * Inherits from Trainer base class and implements Gen 8-specific save file format.
     * Handles automatic decryption on construction and provides Gen 8-specific
     * encryption when updating blocks.
     *
     * Gen 8 Specific Features:
     * - 32 boxes with 30 slots each
     * - PA8 Pokemon format (376 bytes party, 360 bytes stored)
     * - Block-based save file structure
     * - Gen 8 encryption algorithm
     *
     * Save File Structure (Legends: Arceus):
     * - File: "main" (approximately 2.94MB)
     * - Format: Multiple blocks identified by key values
     * - Encryption: Block-level encryption + Pokemon encryption
     */
    class Trainer8LA final : public Trainer
    {
    public:
        // ========================================
        // Constructor
        // ========================================

        /**
         * Constructs a Trainer8LA object from save file blocks.
         *
         * Process:
         * 1. Parses blocks to extract trainer info
         * 2. Decrypts and loads party Pokemon (PA8)
         * 3. Decrypts and loads box Pokemon (PA8)
         * 4. Loads items and box names
         *
         * @param blocks Save file blocks parsed from Gen 8 save file
         */
        // Legends: Arceus (PA8), SwishCrypto SCBlock save. Its box/party slots are PACKED (no inter-slot
        // gap); the named slot-stride members below MIRROR Trainer9LZA (Legends: Z-A) — which GAPS its
        // slots — so the shared parse/serialize logic reads identically across the two classes.
        explicit Trainer8LA(std::vector<Block> blocks) : Trainer(std::move(blocks))
        {
            party.reserve(MAX_PARTY_SLOTS);
            boxes.resize(BOX_COUNT8_LA);
            boxNames.resize(BOX_COUNT8_LA);

            // Parse all blocks to extract data (includes SAVE_REVISION8_LA -> parseSaveRevisionBlock).
            for (const auto& block : this->blocks) {
                parseBlock(block);
            }
        }

        /// Destructor - cleanup handled by base class and unique_ptrs
        ~Trainer8LA() override = default;

        // Delete copy operations
        Trainer8LA(const Trainer8LA&) = delete;
        Trainer8LA& operator=(const Trainer8LA&) = delete;

        // Allow move operations
        Trainer8LA(Trainer8LA&&) noexcept = default;
        Trainer8LA& operator=(Trainer8LA&&) noexcept = default;

        // ========================================
        // Implementation of Pure Virtual Methods
        // ========================================

        /**
         * Updates the PARTY_KEY block with modified Pokemon data.
         * Uses Gen 8 encryption (encryptArray8LA).
         */
        void updatePartyBlock() override;

        /**
         * Updates the BOX_KEY block with modified Pokemon data.
         * Uses Gen 8 encryption (encryptArray8LA).
         */
        void updateBoxBlock() override;
        void updateBoxNameBlock() override;
        void updateCurrentBoxBlock() override;
        bool supportsBoxNames() const noexcept override { return true; }
        size_t getMaxBoxNameLength() const noexcept override { return BOX_NAME_LENGTH8_LA / 2 - 1; }

        /**
         * Updates the ITEM_KEY block with modified inventory data.
         */
        void updateItemBlock() override;

        // Fixed-capacity packed pouches (KeyItems 100 / Stored 180 / Recipes 70), and the general
        // Items bag = min(675, SatchelUpgrades + 20). Enables in-pouch item creation for PLA.
        size_t getItemPouchCapacity(int pouch) const override;

        /**
         * Creates a species-0, checksum-valid blank PA8 entity (mirrors updateBoxBlock()'s
         * encrypted-blank fallback: zeroed party-size SIZE_PARTY8_LA buffer -> encryptArray8LA(seed 0)
         * -> Pokemon8LA, which keeps a party-size buffer). Starting point for the Pokemon creator.
         */
        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        /**
         * Gets the number of boxes available in Gen 8.
         * @return 32 (Legends: Arceus has 32 boxes)
         */
        size_t getBoxCount() const noexcept override {
            return BOX_COUNT8_LA;
        }

        /**
         * Gets the number of slots per box in Gen 8.
         * @return 30 (Legends: Arceus has 30 slots per box, 6x5 grid)
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
         * Gets the game group for Legends: Arceus trainers.
         * @return GameVersion::PLA
         */
        GameVersion getGameGroup() const noexcept override {
            return GameVersion::PLA;
        }

    private:
        // Legends: Arceus PACKS its slots: party slots are SIZE_PARTY8_LA (0x178) and box slots
        // SIZE_STORED8_LA (0x168), stored back-to-back with no inter-slot gap. (Named members mirror
        // Trainer9LZA — which gaps its slots — so the shared parse/serialize logic reads identically.)
        size_t m_partySlotStride = SIZE_PARTY8_LA;  // packed: no gap between party slots
        size_t m_boxSlotStride   = SIZE_STORED8_LA;  // packed: no gap between box slots
        size_t m_slotGapZero     = 0;               // no gap bytes after mon data
        /**
         * Parses a single block to extract relevant data.
         * Called during construction for each block in the save file.
         *
         * @param block The block to parse
         */
        void parseBlock(const Block& block);

        /**
         * Parses the MY_STATUS block (MyStatus8a) for trainer ID + name.
         * Location: ID32 at 0x10 (TID16 0x10 / SID16 0x12), OT name at 0x20 (26 bytes).
         */
        void parseMyStatusBlock(const Block& block);

        /**
         * Parses the PARTY block to extract party Pokemon.
         * Format: 6 slots of SIZE_PARTY8_LA (0x178 = 376 bytes each)
         */
        void parsePartyBlock(const Block& block);

        /**
         * Parses the MISC block to extract money.
         * Location: Money at offset 0x04 (4 bytes)
         */
        void parseMoneyBlock(const Block& block);

        // THERE'S NO TRAINER CARD PARSING IN GEN 8

        /**
         * Parses the ITEM block to extract inventory items.
         * Format: Multiple pouches with variable item counts
         */
        void parseItemBlock(const Block& block);

        /**
         * Parses the BOX block to extract box Pokemon.
         * Format: 32 boxes * 30 slots * SIZE_STORED8_LA (0x168 = 360 bytes each)
         */
        void parseBoxBlock(const Block& block);

        /**
         * Parses the BOX_LAYOUT block to extract box names.
         * Format: 32 names * BOX_NAME_LENGTH (34 bytes, UTF-16LE)
         */
        void parseBoxLayoutBlock(const Block& block);

        /** Parses the CURRENT_BOX block ("U8 Box Index") so the editor opens on the last box used. */
        void parseCurrentBoxBlock(const Block& block);

        /**
         * Parses the SAVE_REVISION block (u64) for version detection. Legends: Arceus shipped without
         * DLC, so only the base-game revision is expected.
         */
        void parseSaveRevisionBlock(const Block& block);
    };
}

#endif
