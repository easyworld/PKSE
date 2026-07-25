/**
 * Trainer7LGPE.h - Generation 7 Let's Go Trainer/Save File Data Management
 *
 * This file defines the Trainer7LGPE class for Pokemon Let's Go Pikachu/Eevee.
 *
 * Trainer7LGPE implements generation-specific logic for:
 * - PB7 Pokemon storage (party and boxes)
 * - Gen 7 block keys (Let's Go format)
 * - Gen 7 encryption (encryptArray7LGPE/decryptArray7LGPE)
 * - Let's Go-specific save file structure
 */

#ifndef TRAINER_TRAINER7_LGPE_H
#define TRAINER_TRAINER7_LGPE_H

#include <cstring>

#include "Trainer/Trainer.h"
#include "Trainer/Inventory7LGPE.h"
#include "Pokemon/Pokemon7LGPE.h"
#include "Encryption/Encryption7LGPE.h"
#include "Save/Block.h"

namespace Trainer {
    // ========================================
    // Generation 7 Let's Go Save File Block Keys
    // ========================================
    // Let's Go uses fixed-offset blocks, not SCBlocks like Gen 8+.
    // These keys are synthetic identifiers for internal use, matching
    // the block indices from PKHeX's BelugaBlockIndex enum.
    //
    // Save file structure:
    // - File: "main" (0xB8800 bytes = 757,760 bytes)
    // - Format: 21 fixed-position blocks
    // - 40 boxes with 25 slots each (1000 total storage)
    // ========================================

    // Block keys == PKHeX BelugaBlockIndex values (used as unique map keys, not offsets).
    // See PKHeX.Core/Saves/Substructures/Gen7/LGPE/BelugaBlockIndex.cs.
    constexpr size_t MY_ITEM7_LGPE = 0x00;              // idx 0  Inventory items
    constexpr size_t MY_STATUS7_LGPE = 0x02;            // idx 2  Trainer info (name, ID, gender)
    constexpr size_t ZUKAN7_LGPE = 0x04;                // idx 4  Pokedex
    constexpr size_t MISC7_LGPE = 0x05;                 // idx 5  Miscellaneous data (money)
    constexpr size_t POKE_LIST_HEADER7_LGPE = 0x08;     // idx 8  Party header (pointers + starter + count)
    constexpr size_t POKE_LIST_POKEMON7_LGPE = 0x09;    // idx 9  Pokemon storage (party + boxes)
    constexpr size_t PLAY_TIME7_LGPE = 0x0A;            // idx 10 Play time

    // Generation 7 Let's Go constants
    constexpr size_t SAVE_SIZE7_LGPE = 0xB8800;         // 757,760 bytes
    constexpr size_t BOX_COUNT7_LGPE = 40;              // Number of boxes
    constexpr size_t SLOTS_PER_BOX7_LGPE = 25;          // Slots per box (different from Gen 8's 30)
    constexpr size_t BOX_NAME_LENGTH7_LGPE = 0x22;      // Box name length (UTF-16LE)
    constexpr size_t SIZE_PARTY7_LGPE = 0x104;          // 260 bytes per Pokemon

    /**
     * Trainer7LGPE - Generation 7 Let's Go Trainer Class
     *
     * Inherits from Trainer base class and implements Let's Go-specific save file format.
     * Handles automatic decryption on construction and provides Gen 7-specific
     * encryption when updating blocks.
     *
     * Gen 7 Let's Go Specific Features:
     * - 40 boxes with 25 slots each (1000 Pokemon storage)
     * - PB7 Pokemon format (260 bytes for all)
     * - Fixed-offset block structure (converted to Block format)
     * - Gen 6/7 style encryption
     * - No breeding/eggs
     *
     * Save File Structure (Let's Go):
     * - File: "main" (0xB8800 bytes = 757,760 bytes)
     * - Format: Fixed-position blocks (converted to Block structures)
     * - Encryption: Gen 6/7 Pokemon encryption
     */
    class Trainer7LGPE final : public Trainer
    {
    public:
        // ========================================
        // Constructor
        // ========================================

        /**
         * Constructs a Trainer7LGPE object from save file blocks.
         *
         * Process:
         * 1. Parses blocks to extract trainer info
         * 2. Decrypts and loads party Pokemon (PB7)
         * 3. Decrypts and loads box Pokemon (PB7)
         * 4. Loads items and box names
         *
         * @param blocks Save file blocks parsed from Let's Go save file
         */
        explicit Trainer7LGPE(std::vector<Save::Block> blocks) : Trainer(std::move(blocks))
        {
            party.reserve(MAX_PARTY_SLOTS);
            boxes.resize(BOX_COUNT7_LGPE);
            boxNames.resize(BOX_COUNT7_LGPE);

            // Parse all blocks to extract data
            for (const auto& block : this->blocks) {
                parseBlock(block);
            }
        }

        /// Destructor - cleanup handled by base class and unique_ptrs
        ~Trainer7LGPE() override = default;

        // Delete copy operations
        Trainer7LGPE(const Trainer7LGPE&) = delete;
        Trainer7LGPE& operator=(const Trainer7LGPE&) = delete;

        // Allow move operations
        Trainer7LGPE(Trainer7LGPE&&) noexcept = default;
        Trainer7LGPE& operator=(Trainer7LGPE&&) noexcept = default;

        // ========================================
        // Implementation of Pure Virtual Methods
        // ========================================

        /**
         * Updates the POKE_LIST_POKEMON block with modified party Pokemon data.
         * Uses Gen 7 encryption (encryptArray7LGPE).
         */
        void updatePartyBlock() override;

        /**
         * Updates the POKE_LIST_POKEMON block with modified box Pokemon data.
         * Uses Gen 7 encryption (encryptArray7LGPE).
         */
        void updateBoxBlock() override;

        /**
         * Updates the MY_ITEM block with modified inventory data.
         */
        void updateItemBlock() override;

        /**
         * Creates a species-0, checksum-valid blank PB7 entity via the encrypt->decrypt round-trip:
         * zeroed SIZE_PARTY7_LGPE buffer -> encryptArray7LGPE(seed 0) -> Pokemon7LGPE. Starting point
         * for the Pokemon creator. (LGPE's own empty box slots are raw zeros because the read path
         * gates on a non-zero EC, but a live blank entity still needs a valid decrypted PB7 buffer.)
         */
        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        /**
         * Gets the number of boxes available in Let's Go.
         * @return 40 (Let's Go has 40 boxes)
         */
        size_t getBoxCount() const noexcept override {
            return BOX_COUNT7_LGPE;
        }

        /**
         * Gets the number of slots per box in Let's Go.
         * @return 25 (Let's Go has 25 slots per box, 5x5 grid)
         */
        size_t getSlotsPerBox() const noexcept override {
            return SLOTS_PER_BOX7_LGPE;
        }

        /**
         * Gets the number of Pokemon currently in the party.
         * @return Party size (0-6)
         */
        size_t getPartySize() const noexcept override {
            return party.size();
        }

        /**
         * Gets the game group for Gen 7 Let's Go trainers.
         * @return GameVersion::GG (Let's Go group)
         */
        GameVersion getGameGroup() const noexcept override {
            return GameVersion::GG;
        }

        /**
         * Checks if a Pokemon at the given storage location is the Partner Pokemon.
         * In Let's Go, this is the Pikachu or Eevee that accompanies the trainer.
         * @param boxIndex Box index (0-based)
         * @param slotIndex Slot index within the box (0-based)
         * @return true if this Pokemon is the Partner Pokemon
         */
        bool isStarterPokemon(size_t boxIndex, size_t slotIndex) const noexcept override {
            constexpr uint16_t SLOT_EMPTY = 1001;
            if (starterIndex == SLOT_EMPTY || starterIndex >= BOX_COUNT7_LGPE * SLOTS_PER_BOX7_LGPE) {
                return false;
            }
            size_t storageIndex = boxIndex * SLOTS_PER_BOX7_LGPE + slotIndex;
            return storageIndex == starterIndex;
        }

        /**
         * Gets the party position of a Pokemon at the given storage location.
         * Let's Go uses an index-based party system where party members reference storage slots.
         * @param boxIndex Box index (0-based)
         * @param slotIndex Slot index within the box (0-based)
         * @return Party position (1-6) if in party, 0 if not in party
         */
        int getPartyPosition(size_t boxIndex, size_t slotIndex) const noexcept override {
            constexpr uint16_t SLOT_EMPTY = 1001;
            size_t storageIndex = boxIndex * SLOTS_PER_BOX7_LGPE + slotIndex;

            for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) {
                if (partyIndices[i] != SLOT_EMPTY && partyIndices[i] == storageIndex) {
                    return static_cast<int>(i + 1);  // Return 1-based position
                }
            }
            return 0;  // Not in party
        }

        /**
         * Checks if a party Pokemon at the given index is the Partner Pokemon.
         * @param partyIndex Party index (0-based, 0-5)
         * @return true if this Pokemon is the Partner Pokemon
         */
        bool isPartyPokemonStarter(size_t partyIndex) const noexcept override {
            constexpr uint16_t SLOT_EMPTY = 1001;
            if (partyIndex >= MAX_PARTY_SLOTS || starterIndex == SLOT_EMPTY) {
                return false;
            }
            return partyIndices[partyIndex] == starterIndex;
        }

        /**
         * Swaps two box slots AND updates the LGPE storage-index pointers (party members and
         * the starter reference slots by index box*25+slot), so the party/starter follow their
         * Pokemon to the new slot. Storage stays gapless (both slots remain occupied).
         */
        void swapBoxSlots(size_t b1, size_t s1, size_t b2, size_t s2) override {
            const uint16_t n1 = static_cast<uint16_t>(b1 * SLOTS_PER_BOX7_LGPE + s1);
            const uint16_t n2 = static_cast<uint16_t>(b2 * SLOTS_PER_BOX7_LGPE + s2);
            for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) {
                if (partyIndices[i] == n1)      partyIndices[i] = n2;
                else if (partyIndices[i] == n2) partyIndices[i] = n1;
            }
            if (starterIndex == n1)      starterIndex = n2;
            else if (starterIndex == n2) starterIndex = n1;
            Trainer::swapBoxSlots(b1, s1, b2, s2);
        }

        /**
         * Re-packs the gapless storage list in memory and remaps the party/partner pointers, so the
         * editor shows what will actually be written. See Trainer::compactStorage.
         */
        bool compactStorage() override;

        /**
         * Mirrors an edited party member's box slot into its party copy (and the reverse). LGPE keeps
         * a party member as BOTH a box/storage slot and an independent party copy, and updateBoxBlock()
         * overlays the party copy onto the box slot on save ("party wins") — so a box-slot edit is
         * silently lost unless propagated here. Copies the full decrypted buffer (both are PK7b, same
         * size), so the edit's already-recomputed stats/checksum come along. No-op if the slot isn't a
         * party member. See Trainer::mirrorPartyMemberFromBox.
         */
        void mirrorPartyMemberFromBox(size_t boxIndex, size_t slotIndex) override;
        void mirrorPartyMemberFromParty(size_t partyIndex) override;

    private:
        /**
         * Parses a single block to extract relevant data.
         * Called during construction for each block in the save file.
         *
         * @param block The block to parse
         */
        void parseBlock(const Block& block);

        /**
         * Parses the MY_STATUS block to extract trainer info.
         * Location: Name at offset 0x00 (26 bytes, UTF-16LE)
         * Location: ID32 at offset 0xA0 (4 bytes)
         */
        void parseMyStatusBlock(const Block& block);

        /**
         * Parses the POKE_LIST_HEADER block to get party count.
         * Location: Party count at offset 0x00 (1 byte)
         */
        void parsePokeListHeaderBlock(const Block& block);

        /**
         * Parses the POKE_LIST_POKEMON block to extract party and box Pokemon.
         * Format: 6 party slots + 40 boxes * 25 slots of SIZE_PARTY7_LGPE (260 bytes each)
         */
        void parsePokeListPokemonBlock(const Block& block);

        /**
         * Parses the MISC block to extract money.
         * Location: Money at offset 0x04 (4 bytes)
         */
        void parseMiscBlock(const Block& block);

        /**
         * Parses the PLAY_TIME block to extract play time.
         * Location: Hours at offset 0x00 (2 bytes)
         * Location: Minutes at offset 0x02 (1 byte)
         * Location: Seconds at offset 0x03 (1 byte)
         */
        void parsePlayTimeBlock(const Block& block);

        /**
         * Parses the MY_ITEM block to extract inventory items.
         * Items are organized into pouches (Medicine, TMs, Candy, etc.)
         */
        void parseMyItemBlock(const Block& block);

        /// Party count from POKE_LIST_HEADER block
        uint8_t partyCount = 0;

        /// Party indices - which storage slots are party members (LGPE uses index-based party)
        /// SLOT_EMPTY (1001) marks empty positions
        uint16_t partyIndices[MAX_PARTY_SLOTS] = {1001, 1001, 1001, 1001, 1001, 1001};

        /// Starter Pokemon index in storage
        uint16_t starterIndex = 1001;
    };

    /**
     * Creates blocks from raw Let's Go save data.
     * This function converts the fixed-offset save format to Block structures
     * for consistent handling with other generations.
     *
     * @param saveData Raw save file data (0xB8800 bytes)
     * @return Vector of Block structures
     */
    std::vector<Save::Block> createBlocksFromSaveData7LGPE(const std::vector<uint8_t>& saveData);

    /**
     * Serializes edited blocks back into a raw Let's Go save buffer (the full 1MB file),
     * patching each block's bytes at its fixed offset and recomputing its CRC-16/ARC block
     * checksum into the "BEEF" footer. The inverse of createBlocksFromSaveData7LGPE.
     *
     * @param raw    Full save file buffer (>= 0xB8800 bytes); modified in place.
     * @param blocks Blocks (from Trainer::getBlocks()) to write back.
     */
    void writeBlocksToSaveData7LGPE(std::vector<uint8_t>& raw, const std::vector<Save::Block>& blocks);
}

#endif
