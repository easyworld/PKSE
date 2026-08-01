/**
 * Trainer8SWSH.cpp - Generation 8 Trainer Implementation
 *
 * This file implements the Trainer8SWSH class for Pokemon Sword/Shield save files.
 * Handles Gen 8-specific block parsing, Pokemon encryption/decryption, and
 * save file serialization.
 */
#include <algorithm>
#include <cstring>

#include "Trainer/Trainer8SWSH.h"
#include "Trainer/Inventory8SWSH.h"
#include "Pokemon/Pokemon8SWSH.h"
#include "Utils/Logger.h"

using namespace Utils;
using namespace Pokemon;

namespace Trainer {
    // ========================================
    // Block Parsing Methods
    // ========================================

    void Trainer8SWSH::parseBlock(const Block& block)
    {
        switch (block.key) {
            case MY_STATUS8_SWSH:
                parseMyStatusBlock(block);
                break;
            case PARTY8_SWSH:
                parsePartyBlock(block);
                break;
            case MONEY8_SWSH:
                parseMoneyBlock(block);
                break;
            case TRAINER_CARD8_SWSH:
                parseTrainerCardBlock(block);
                break;
            case ITEM8_SWSH:
                parseItemBlock(block);
                break;
            case BOX8_SWSH:
                parseBoxBlock(block);
                break;
            case BOX_LAYOUT8_SWSH:
                parseBoxLayoutBlock(block);
                break;
            case CURRENT_BOX8_SWSH:
                parseCurrentBoxBlock(block);
                break;
            // Additional blocks can be handled here
            default:
                // Unknown block - skip
                break;
        }
    }

    void Trainer8SWSH::parseMyStatusBlock(const Block& block)
    {
        /**
         * MY_STATUS Block Structure:
         * 0xA0: ID32 (4 bytes) - Combined TID16 and SID16
         *
         * ID32 format: SID16 << 16 | TID16
         * Display TID: ID32 % 1000000
         * Display SID: ID32 / 1000000
         */
        if (block.data.size() < 0xA0 + 4) {
            logInfoToFile("Insufficient data for UInt32 at offset 0xA0 in MY_STATUS block");
            return;
        }

        this->ID32 = readUInt32LittleEndian(&block.data[0xA0]);
        this->TID16 = readUInt16LittleEndian(&block.data[0xA0]);
        this->SID16 = readUInt16LittleEndian(&block.data[0xA2]);
        this->TID = this->ID32 % 1000000;
        this->SID = this->ID32 / 1000000;

        // OT name (0xB0, 26 bytes) and gender (0xA5) come from MyStatus8 -- the authoritative source
        // PKHeX uses (SAV8SWSH.OT/Gender => MyStatus, alongside ID32 at 0xA0). They were previously read
        // from the Trainer Card block (a display copy), whose byte at 0xA5 is NOT the gender field, so the
        // trainer gender shown could be a wrong/garbage value.
        if (block.data.size() >= 0xB0 + 0x1A)
            this->trainerName = utf16ToUtf8(getString(&block.data[0xB0], 0x1A));
        if (block.data.size() > 0xA5)
            this->trainerGender = block.data[0xA5] & 1;   // 0xA5: gender (0=M, 1=F)
    }

    void Trainer8SWSH::parsePartyBlock(const Block& block)
    {
        /**
         * PARTY Block Structure:
         * Pokemon stored sequentially at offsets:
         * - Slot 0: offset 0
         * - Slot 1: offset SIZE_PARTY8_SWSH (344 bytes)
         * - Slot 2: offset 2 * SIZE_PARTY8_SWSH
         * - ... up to 6 slots
         *
         * Each slot is SIZE_PARTY8_SWSH bytes (344 bytes for party Pokemon).
         * Empty slots are zeroed out.
         */
        const std::span<const std::byte> blockSpan(
            reinterpret_cast<const std::byte*>(block.data.data()),
            block.data.size()
        );

        for (size_t slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
        {
            const size_t offset = slot * SIZE_PARTY8_SWSH;
            if (offset + SIZE_PARTY8_SWSH > block.data.size())
                break;

            std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_PARTY8_SWSH);

            // Check if slot has valid Pokemon data (non-zero species)
            // The species ID is at offset 0x08 after decryption, but we can check
            // for an all-zero slot to skip empty slots
            bool isEmptySlot = true;
            for (size_t i = 0; i < SIZE_PARTY8_SWSH && i < slotSpan.size(); ++i) {
                if (slotSpan[i] != std::byte{0}) {
                    isEmptySlot = false;
                    break;
                }
            }

            if (!isEmptySlot) {
                // Decrypt and create Pokemon8SWSH object as unique_ptr
                // Pokemon8SWSH constructor handles decryption automatically
                party.push_back(std::make_unique<Pokemon8SWSH>(slotSpan));
            }
        }
    }

    void Trainer8SWSH::parseMoneyBlock(const Block& block)
    {
        /**
         * MONEY Block Structure:
         * 0x04: Money (4 bytes) - Trainer's currency amount
         */
        if (block.data.size() < 0x04 + 4) {
            return;
        }

        this->money = readUInt32LittleEndian(&block.data[0x04]);
    }

    void Trainer8SWSH::parseTrainerCardBlock(const Block& block)
    {
        /**
         * TRAINER_CARD Block Structure:
         * 0x00: Trainer Name (26 bytes, UTF-16LE)
         * 0x1C: Trainer ID (4 bytes) - Legacy trainer ID format
         */
        // OT name and gender are read from MyStatus8 (the authoritative block), not from this display
        // copy -- see parseMyStatusBlock. Nothing else in the Trainer Card is consumed yet.
        (void)block;
    }

    void Trainer8SWSH::parseItemBlock(const Block& block)
    {
        /**
         * ITEM Block Structure:
         * Multiple "pouches" (categories) of items:
         * - Medicine
         * - Balls
         * - Battle Items
         * - Berries
         * - TMs/TRs
         * - Treasures
         * - Ingredients
         * - Key Items
         * - Other
         *
         * Each pouch has a fixed offset and maximum item count.
         * Items are stored as 4-byte values: (count << 16) | itemId
         */
        // Initialize items vector with pouches for each type
        items.resize(static_cast<size_t>(PouchType8SWSH::Count));

        // Load each pouch
        for (int p = 0; p < static_cast<int>(PouchType8SWSH::Count); p++) {
            PouchType8SWSH pouchType = static_cast<PouchType8SWSH>(p);
            const PouchInfo8SWSH& info = getPouchInfo8SWSH(pouchType);

            std::vector<InventoryItem> pouch;
            pouch.reserve(info.maxCount);

            // Read items from block data
            for (int i = 0; i < info.maxCount; i++) {
                size_t offset = info.offset + (i * 4);
                if (offset + 4 <= block.data.size()) {
                    uint32_t itemValue = readUInt32LittleEndian(&block.data[offset]);
                    InventoryItem8SWSH item = InventoryItem8SWSH::fromValue(itemValue);

                    // Only add items with valid IDs (non-zero)
                    if (item.itemId != 0) {
                        pouch.push_back(item);
                    }
                }
            }

            items[p] = std::move(pouch);
        }
    }

    void Trainer8SWSH::parseBoxBlock(const Block& block)
    {
        /**
         * BOX Block Structure:
         * Pokemon stored sequentially for all boxes and slots:
         * - Box 0, Slot 0: offset 0
         * - Box 0, Slot 1: offset SIZE_PARTY8_SWSH
         * - ... Box 0, Slot 29: offset 29 * SIZE_PARTY8_SWSH
         * - Box 1, Slot 0: offset 30 * SIZE_PARTY8_SWSH
         * - ... etc for all 32 boxes
         *
         * Total size: 32 boxes * 30 slots * 344 bytes = 331,776 bytes
         */
        const std::span<const std::byte> blockSpan(
            reinterpret_cast<const std::byte*>(block.data.data()),
            block.data.size()
        );

        for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_SWSH; ++boxIndex) {
            for (size_t slot = 0; slot < BOX_SLOTS; ++slot) {
                // Calculate offset: (boxIndex * slots per box + slot) * bytes per pokemon
                const size_t offset = (boxIndex * BOX_SLOTS + slot) * SIZE_PARTY8_SWSH;
                if (offset + SIZE_PARTY8_SWSH > block.data.size()) {
                    break;
                }

                std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_PARTY8_SWSH);

                // Check if slot has a Pokemon (non-zero data)
                bool isEmptySlot = true;
                for (size_t i = 0; i < SIZE_PARTY8_SWSH && i < slotSpan.size(); ++i) {
                    if (slotSpan[i] != std::byte{0}) {
                        isEmptySlot = false;
                        break;
                    }
                }

                if (!isEmptySlot) {
                    // Decrypt and create Pokemon8SWSH object
                    boxes[boxIndex][slot] = std::make_unique<Pokemon8SWSH>(slotSpan);
                } else {
                    // Empty slot
                    boxes[boxIndex][slot] = nullptr;
                }
            }
        }
    }

    void Trainer8SWSH::parseBoxLayoutBlock(const Block& block)
    {
        /**
         * BOX_LAYOUT Block Structure:
         * Box names stored sequentially:
         * - Box 0 name: offset 0 (34 bytes, UTF-16LE)
         * - Box 1 name: offset 34
         * - ... for all 32 boxes
         *
         * Each name is BOX_NAME_LENGTH8_SWSH bytes (34 bytes).
         */
        for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_SWSH; ++boxIndex) {
            size_t offset = boxIndex * BOX_NAME_LENGTH8_SWSH;
            if (offset + BOX_NAME_LENGTH8_SWSH <= block.data.size()) {
                // Extract box name (UTF-16LE string)
                std::u16string boxNameU16 = getString(
                    block.data.data() + offset,
                    BOX_NAME_LENGTH8_SWSH
                );
                std::string boxName = utf16ToUtf8(boxNameU16);

                // If box name is empty, use default
                if (boxName.empty()) {
                    boxName = "盒子 " + std::to_string(boxIndex + 1);
                }

                boxNames[boxIndex] = boxName;
            } else {
                // Default name if data is insufficient
                boxNames[boxIndex] = "盒子 " + std::to_string(boxIndex + 1);
            }
        }
    }

    void Trainer8SWSH::detectSaveRevision()
    {
        /**
         * Detects the save revision (DLC version) by checking for DLC Pokedex blocks.
         *
         * Revision detection logic:
         * - If SAVE_REVISION8_R2_SWSH block exists with data -> Crown Tundra (revision 2)
         * - If SAVE_REVISION8_R1_SWSH block exists with data -> Isle of Armor (revision 1)
         * - Otherwise -> Base game (revision 0)
         *
         * Note: These blocks contain Pokedex data for each region, and their presence indicates DLC access.
         */
        bool hasR2 = false;
        bool hasR1 = false;

        for (const auto& block : this->blocks) {
            if (block.key == SAVE_REVISION8_R2_SWSH && !block.data.empty()) {
                hasR2 = true;
            }
            if (block.key == SAVE_REVISION8_R1_SWSH && !block.data.empty()) {
                hasR1 = true;
            }
        }

        if (hasR2) {
            this->saveRevision = 2;
            this->saveRevisionString = "Crown Tundra";
            this->gameVersionString = "v1.3";  // Crown Tundra requires v1.3.0+
        } else if (hasR1) {
            this->saveRevision = 1;
            this->saveRevisionString = "Isle of Armor";
            this->gameVersionString = "v1.2";  // Isle of Armor requires v1.2.0+
        } else {
            this->saveRevision = 0;
            this->saveRevisionString = "Base";
            this->gameVersionString = "v1.0";  // Base game v1.0.x - v1.1.x
        }

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Detected save revision: %d (%s, %s)",
            this->saveRevision, this->saveRevisionString.c_str(), this->gameVersionString.c_str());
        logInfoToFile(buffer);
    }

    // ========================================
    // Block Update Methods
    // ========================================

    void Trainer8SWSH::updatePartyBlock()
    {
        /**
         * Updates the PARTY block with modified Pokemon data.
         *
         * Process:
         * 1. Find the PARTY block
         * 2. Ensure block is large enough (6 slots * SIZE_PARTY8_SWSH)
         * 3. For each party Pokemon:
         *    a. Get encryption constant from Pokemon data
         *    b. Encrypt Pokemon data using encryptArray8
         *    c. Write encrypted data to block
         * 4. Zero out empty slots
         */
        for (auto& block : blocks) {
            if (block.key == PARTY8_SWSH) {
                // Ensure the block data is large enough
                size_t requiredSize = MAX_PARTY_SLOTS * SIZE_PARTY8_SWSH;
                if (block.data.size() < requiredSize) {
                    block.data.resize(requiredSize, 0);
                }

                // Write each party Pokemon
                for (size_t i = 0; i < party.size() && i < MAX_PARTY_SLOTS; ++i) {
                    const size_t offset = i * SIZE_PARTY8_SWSH;

                    if (party[i] && party[i]->speciesID() != 0) {
                        // Pokemon exists - encrypt and write
                        const ::Pokemon::Pokemon* pokemon = party[i].get();
                        uint32_t ec = readUInt32LittleEndian(
                            reinterpret_cast<const uint8_t*>(pokemon->getData().data())
                        );

                        // Create span of decrypted Pokemon data
                        std::span<const std::byte> decryptedSpan(
                            pokemon->getData().data(),
                            pokemon->getDataSize()
                        );

                        // Encrypt the Pokemon data
                        std::byte* encryptedData = encryptArray8SWSH(decryptedSpan, ec);

                        // Write encrypted data to block
                        std::memcpy(&block.data[offset], encryptedData, pokemon->getDataSize());

                        // Clean up encrypted buffer
                        delete[] encryptedData;
                    } else {
                        // Empty slot - write zeros
                        std::memset(&block.data[offset], 0, SIZE_PARTY8_SWSH);
                    }
                }

                // Zero out any remaining slots
                for (size_t i = party.size(); i < MAX_PARTY_SLOTS; ++i) {
                    const size_t offset = i * SIZE_PARTY8_SWSH;
                    std::memset(&block.data[offset], 0, SIZE_PARTY8_SWSH);
                }

                break;
            }
        }
    }

    void Trainer8SWSH::updateBoxNameBlock()
    {
        /**
         * The inverse of parseBoxLayoutBlock: 32 names of 0x22 bytes, UTF-16LE, null-terminated and
         * zero-padded (Utils::setString does both, reserving the last slot for the terminator).
         *
         * Deliberately does NOT resize the block the way updateBoxBlock does. BOX_LAYOUT is a fixed
         * 32 * 0x22 region in any real save, so a short block means the save is wrong -- growing it
         * would paper over that and hand the game a block of an unexpected size.
         */
        for (auto& block : blocks) {
            if (block.key != BOX_LAYOUT8_SWSH) continue;
            for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_SWSH && boxIndex < boxNames.size(); ++boxIndex) {
                if (!isBoxNameDirty(boxIndex)) continue;   // never persist a display default
                const size_t offset = boxIndex * BOX_NAME_LENGTH8_SWSH;
                if (offset + BOX_NAME_LENGTH8_SWSH > block.data.size()) break;
                setString(block.data.data() + offset, BOX_NAME_LENGTH8_SWSH,
                          utf8ToUtf16(boxNames[boxIndex]), MAX_BOX_NAME_CHARS8_SWSH);
            }
            break;
        }
    }

    void Trainer8SWSH::parseCurrentBoxBlock(const Block& block)
    {
        // "U32 Box Index" -- a scalar block; PKHeX reads it as a byte (0..31 fits one byte).
        if (block.data.empty()) return;
        uint8_t box = block.data[0];
        if (box < BOX_COUNT8_SWSH) this->currentBox = box;
    }

    void Trainer8SWSH::updateCurrentBoxBlock()
    {
        // Inverse of parseCurrentBoxBlock: write the low byte and clear the rest of the scalar.
        for (auto& block : blocks) {
            if (block.key != CURRENT_BOX8_SWSH || block.data.empty()) continue;
            block.data[0] = static_cast<uint8_t>(currentBox);
            for (size_t i = 1; i < block.data.size() && i < 4; ++i) block.data[i] = 0;
            break;
        }
    }

    void Trainer8SWSH::updateBoxBlock()
    {
        /**
         * Updates the BOX block with modified Pokemon data.
         *
         * Process similar to updatePartyBlock, but for all boxes:
         * 1. Find the BOX block
         * 2. Ensure block is large enough (32 boxes * 30 slots * SIZE_PARTY8_SWSH)
         * 3. For each box and slot:
         *    a. If Pokemon exists, encrypt and write
         *    b. If slot is empty, write zeros
         */
        for (auto& block : blocks) {
            if (block.key == BOX8_SWSH) {
                // Ensure the block data is large enough for all boxes
                size_t requiredSize = BOX_COUNT8_SWSH * BOX_SLOTS * SIZE_PARTY8_SWSH;
                if (block.data.size() < requiredSize) {
                    block.data.resize(requiredSize, 0);
                }

                // Empty box slots in the real save are an ENCRYPTED blank that DECRYPTS to species 0 —
                // NOT literal zeros. The game decrypts every box slot and checks species; a zeroed slot
                // decrypts to garbage and renders a BAD EGG. Reuse the game's own blank: copy the raw
                // bytes of an existing empty (species-0) slot; synthesize one if every box is full.
                std::vector<uint8_t> blankSlot;
                for (size_t bi = 0; bi < BOX_COUNT8_SWSH && blankSlot.empty(); ++bi) {
                    for (size_t s = 0; s < BOX_SLOTS; ++s) {
                        if (boxes[bi][s] && boxes[bi][s]->speciesID() == 0) {
                            const size_t off = (bi * BOX_SLOTS + s) * SIZE_PARTY8_SWSH;
                            if (off + SIZE_PARTY8_SWSH <= block.data.size()) {
                                blankSlot.assign(block.data.begin() + off,
                                                 block.data.begin() + off + SIZE_PARTY8_SWSH);
                                break;
                            }
                        }
                    }
                }
                if (blankSlot.empty()) {
                    std::vector<std::byte> zero(SIZE_PARTY8_SWSH, std::byte{0});
                    std::byte* enc = encryptArray8SWSH(
                        std::span<const std::byte>(zero.data(), SIZE_PARTY8_SWSH), 0);
                    blankSlot.resize(SIZE_PARTY8_SWSH);
                    for (size_t k = 0; k < SIZE_PARTY8_SWSH; ++k)
                        blankSlot[k] = static_cast<uint8_t>(enc[k]);
                    delete[] enc;
                }

                // Write each Pokemon back to the block
                for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_SWSH; ++boxIndex) {
                    for (size_t slot = 0; slot < BOX_SLOTS; ++slot) {
                        const size_t offset = (boxIndex * BOX_SLOTS + slot) * SIZE_PARTY8_SWSH;

                        // Gate on species, not just the pointer (matches Trainer9LZA/Trainer7LGPE). A
                        // non-null but blank/species-0 slot (a "ghost" slot loaded from the save) must
                        // be zeroed to read as EMPTY in-game — re-encrypting a blank writes a bad egg.
                        if (boxes[boxIndex][slot] && boxes[boxIndex][slot]->speciesID() != 0) {
                            // Pokemon exists - encrypt and write
                            const auto& pokemon = boxes[boxIndex][slot];

                            // Get the Encryption Constant (used as seed for encryption)
                            uint32_t ec = readUInt32LittleEndian(
                                reinterpret_cast<const uint8_t*>(pokemon->getData().data())
                            );

                            // Create span of decrypted Pokemon data
                            std::span<const std::byte> decryptedSpan(
                                pokemon->getData().data(),
                                pokemon->getDataSize()
                            );

                            // Encrypt the Pokemon data
                            std::byte* encryptedData = encryptArray8SWSH(decryptedSpan, ec);

                            // Write encrypted data to block
                            std::memcpy(&block.data[offset], encryptedData, pokemon->getDataSize());

                            // Clean up encrypted buffer
                            delete[] encryptedData;
                        } else {
                            // Empty/cleared slot: write the game's encrypted blank, NOT zeros (zeros
                            // decrypt to garbage in-game and show as a BAD EGG in every empty slot).
                            std::memcpy(&block.data[offset], blankSlot.data(), SIZE_PARTY8_SWSH);
                        }
                    }
                }
                break;
            }
        }
    }

    std::unique_ptr<::Pokemon::Pokemon> Trainer8SWSH::createBlankPokemon() const
    {
        // Mirror updateBoxBlock()'s encrypted-blank fallback: a zeroed *decrypted* PK8 buffer
        // encrypted with EC/seed 0, then fed to the ctor (which decrypts it straight back to zeros)
        // -> a clean species-0, Sanity-0, checksum-valid entity. Raw zeros in the ctor would decrypt
        // to garbage (BAD EGG); the encrypt->decrypt round-trip is what makes the blank valid.
        std::vector<std::byte> zero(SIZE_PARTY8_SWSH, std::byte{0});
        std::byte* enc = encryptArray8SWSH(
            std::span<const std::byte>(zero.data(), SIZE_PARTY8_SWSH), 0);
        auto p = std::make_unique<Pokemon8SWSH>(
            std::span<const std::byte>(enc, SIZE_PARTY8_SWSH));
        delete[] enc;
        return p;
    }

    void Trainer8SWSH::updateItemBlock()
    {
        /**
         * Updates the ITEM block with modified inventory data.
         *
         * Process:
         * 1. Find the ITEM block
         * 2. Ensure block is large enough for all pouches
         * 3. For each pouch:
         *    a. Write items to their designated offsets
         *    b. Zero out remaining slots
         */
        for (auto& block : blocks) {
            if (block.key == ITEM8_SWSH) {
                // Ensure the block data is large enough
                size_t maxSize = 4856; // Sum of all pouch sizes * 4 bytes per item
                if (block.data.size() < maxSize) {
                    block.data.resize(maxSize, 0);
                }

                // Write each pouch back to the block
                for (int p = 0; p < static_cast<int>(PouchType8SWSH::Count); p++) {
                    PouchType8SWSH pouchType = static_cast<PouchType8SWSH>(p);
                    const PouchInfo8SWSH& info = getPouchInfo8SWSH(pouchType);
                    const auto& pouch = items[p];

                    // Write items to block
                    int itemIndex = 0;
                    for (const auto& item : pouch) {
                        size_t offset = info.offset + (itemIndex * 4);
                        if (offset + 4 <= block.data.size()) {
                            
                            // Convert from InventoryItem (base class) to InventoryItem8SWSH
                            InventoryItem8SWSH item8;
                            item8.itemId = item.itemId;
                            item8.count = item.count;
                            item8.isNew = item.isNew;
                            item8.isFavorite = item.isFavorite;

                            uint32_t itemValue = item8.toValue();
                            writeUInt32LittleEndian(&block.data[offset], itemValue);
                        }
                        itemIndex++;
                    }

                    // Zero out remaining slots in this pouch
                    for (int i = itemIndex; i < info.maxCount; i++) {
                        size_t offset = info.offset + (i * 4);
                        if (offset + 4 <= block.data.size()) {
                            writeUInt32LittleEndian(&block.data[offset], 0);
                        }
                    }
                }
                break;
            }
        }
    }
}
