/**
 * Trainer9LZA.cpp - Generation 9 Trainer Implementation
 *
 * This file implements the Trainer9LZA class for Pokemon Legends: Z-A save files.
 * Handles Gen 9-specific block parsing, Pokemon encryption/decryption, and
 * save file serialization.
 */
#include <algorithm>
#include <cstring>

#include "Trainer/Trainer9LZA.h"
#include "Trainer/Inventory9LZA.h"
#include "Names/ItemPouches.h"   // getPouchItems -- per-pouch legal ids
#include "Utils/Logger.h"

using namespace Utils;

namespace Trainer {
    // ========================================
    // Block Parsing Methods
    // ========================================

    void Trainer9LZA::parseBlock(const Block& block)
    {
        switch (block.key) {
            case MY_STATUS9_LZA:
                parseMyStatusBlock(block);
                break;
            case PARTY9_LZA:
                parsePartyBlock(block);
                break;
            case MONEY9_LZA:
                parseMoneyBlock(block);
                break;
            case ITEM9_LZA:
                parseItemBlock(block);
                break;
            case BOX9_LZA:
                parseBoxBlock(block);
                break;
            case BOX_LAYOUT9_LZA:
                parseBoxLayoutBlock(block);
                break;
            case CURRENT_BOX9_LZA:
                parseCurrentBoxBlock(block);
                break;
            case SAVE_REVISION9_LZA:
                parseSaveRevisionBlock(block);
                break;
            // Additional blocks can be handled here
            default:
                // Unknown block - skip
                break;
        }
    }

    void Trainer9LZA::parseMyStatusBlock(const Block& block)
    {
        /**
         * MY_STATUS Block Structure:
         * 0x00: ID32 (4 bytes) - Combined TID16 and SID16
         *
         * ID32 format: SID16 << 16 | TID16
         * Display TID: ID32 % 1000000
         * Display SID: ID32 / 1000000
         */
        if (block.data.size() < 0x00 + 4) {
            logInfoToFile("Insufficient data for UInt32 at offset 0x00 in MY_STATUS block");
            return;
        }

        this->ID32 = readUInt32LittleEndian(&block.data[0x00]);
        this->TID16 = readUInt16LittleEndian(&block.data[0x00]);
        this->SID16 = readUInt16LittleEndian(&block.data[0x02]);
        this->TID = this->ID32 % 1000000;
        this->SID = this->ID32 / 1000000;
        // OT name is a 26-byte (0x1A) field at 0x10 -- PKHeX MyStatus9.OriginalTrainerTrash =
        // Data.Slice(0x10, 0x1A). The old min(0x10, 0x1A) mistakenly used the OFFSET (0x10 = 16 bytes =
        // 8 code units) as the length, truncating any trainer name longer than 8 characters.
        if (block.data.size() >= 0x10 + 0x1A)
            this->trainerName = utf16ToUtf8(getString(&block.data[0x10], 0x1A));
        this->trainerGender = block.data[0x05] & 1;   // 0x05: gender (0=M, 1=F)
        logInfoToFile("Parsed Trainer Name", this->trainerName.c_str());
    }

    void Trainer9LZA::parsePartyBlock(const Block& block)
    {
        /**
         * PARTY Block Structure (Pokemon Legends Z-A):
         * Pokemon stored with gaps between slots:
         * - Slot 0: offset 0 (SIZE_PARTY9_LZA bytes of data + GAP_BOX_SLOT9_LZA gap)
         * - Slot 1: offset PARTY_SLOT_SIZE9_LZA (480 bytes)
         * - Slot 2: offset 2 * PARTY_SLOT_SIZE9_LZA
         * - ... up to 6 slots
         *
         * Each slot spans PARTY_SLOT_SIZE9_LZA bytes (480 bytes), but only the first
         * SIZE_PARTY9_LZA bytes (344 bytes) contain Pokemon data. The remaining gap
         * (GAP_BOX_SLOT9_LZA = 0x88 bytes) is unused/padding.
         */
        const std::span<const std::byte> blockSpan(reinterpret_cast<const std::byte*>(block.data.data()), block.data.size());

        for (size_t slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
        {
            // Calculate offset to this slot (packed for S/V, gapped for Z-A)
            const size_t offset = slot * m_partySlotStride;
            if (offset + SIZE_PARTY9_LZA > block.data.size())
                break;

            // Extract only the Pokemon data portion (SIZE_PARTY9_LZA bytes)
            std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_PARTY9_LZA);

            // Check if slot has valid Pokemon data (non-zero species)
            // The species ID is at offset 0x08 after decryption, but we can check
            // for an all-zero slot to skip empty slots
            bool isEmptySlot = true;
            for (size_t i = 0; i < SIZE_PARTY9_LZA && i < slotSpan.size(); ++i) {
                if (slotSpan[i] != std::byte{0}) {
                    isEmptySlot = false;
                    break;
                }
            }

            if (!isEmptySlot) {
                // Decrypt and create Pokemon9LZA object as unique_ptr
                // Pokemon9LZA constructor handles decryption automatically
                party.push_back(std::make_unique<Pokemon9LZA>(slotSpan));
            }
        }
    }

    void Trainer9LZA::parseMoneyBlock(const Block& block)
    {
        /**
         * MONEY Block (Gen 9 Legends Z-A):
         * For Gen 9 Legends Z-A, the money value is stored directly as the block value.
         * 0x00: Money (4 bytes)
         */
        if (block.data.size() < 4) {
            return;
        }

        this->money = readUInt32LittleEndian(block.data.data());
    }

    void Trainer9LZA::parseItemBlock(const Block& block)
    {
        /**
         * ITEM Block Structure (Gen 9 Legends Z-A):
         * Items are stored BY ITEM ID as index!
         * - Item at ID X is at offset (X * 0x10)
         * - Block size: 0xBB80 bytes (47,872 bytes)
         * - Each item: 0x10 bytes (16 bytes)
         *
         * Structure per item (16 bytes):
         * 0x00-0x03: Pouch ID (uint32) - which pouch this belongs to
         * 0x04-0x07: Count (int32) - quantity
         * 0x08-0x0B: Flags (uint32) - isNew, isFavorite, etc.
         * 0x0C-0x0F: Padding
         */

        // Initialize items vector with pouches for each type
        items.resize(static_cast<size_t>(POUCH_COUNT9_LZA));

        // For each pouch, iterate through valid item IDs
        for (int i = 0; i < static_cast<int>(POUCH_COUNT9_LZA); i++) {
            items[i].clear();

            // Legal ids for this pouch, from the generated PKHeX-derived table. This replaced a
            // hand-written list that was byte-identical to the other Gen 9 game's and wrong for
            // both -- it bucketed vitamins under Battle Items and claimed pockets the
            // game does not have. Pouch membership is display-only: the write below is keyed on
            // item id, not pouch.
            const auto validIds = Names::getPouchItems(Enums::GameVersion::ZA, static_cast<size_t>(i));

            for (uint16_t itemId : validIds) {
                // Calculate offset: itemID * 0x10
                size_t offset = itemId * ITEM_SIZE9_LZA;

                // Make sure we're within bounds
                if (offset + ITEM_SIZE9_LZA > block.data.size()) {
                    continue;
                }

                // Read the item data at this index
                InventoryItem9LZA item = InventoryItem9LZA::fromBytes(itemId, &block.data[offset]);

                // Only add if count > 0
                if (item.count > 0) {
                    items[i].push_back(item);
                }
            }
        }
    }

    void Trainer9LZA::parseBoxBlock(const Block& block)
    {
        /**
         * BOX Block Structure:
         * Pokemon stored with gaps between slots:
         * - Box 0, Slot 0: offset 0 (SIZE_PARTY9_LZA bytes of data + GAP_BOX_SLOT9_LZA gap)
         * - Box 0, Slot 1: offset BOX_SLOT_SIZE9_LZA (408 bytes)
         * - ... Box 0, Slot 29: offset 29 * BOX_SLOT_SIZE9_LZA
         * - Box 1, Slot 0: offset 30 * BOX_SLOT_SIZE9_LZA
         * - ... etc for all 32 boxes
         *
         * Each slot spans BOX_SLOT_SIZE9_LZA bytes (408 bytes), but only the first
         * SIZE_PARTY9_LZA bytes (344 bytes) contain Pokemon data. The remaining gap
         * (GAP_BOX_SLOT9_LZA = 0x40 bytes) is unused/padding.
         *
         * Total size: 32 boxes * 30 slots * 408 bytes = 391,680 bytes
         */
        const std::span<const std::byte> blockSpan(
            reinterpret_cast<const std::byte*>(block.data.data()),
            block.data.size()
        );

        for (size_t boxIndex = 0; boxIndex < BOX_COUNT9_LZA; ++boxIndex) {
            for (size_t slot = 0; slot < BOX_SLOTS; ++slot) {
                // Calculate offset (packed for S/V, gapped for Z-A)
                const size_t offset = (boxIndex * BOX_SLOTS + slot) * m_boxSlotStride;
                if (offset + SIZE_PARTY9_LZA > block.data.size()) {
                    break;
                }

                // Extract only the Pokemon data portion (SIZE_PARTY9_LZA bytes)
                std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_PARTY9_LZA);

                // Check if slot has a Pokemon (non-zero data)
                bool isEmptySlot = true;
                for (size_t i = 0; i < SIZE_PARTY9_LZA && i < slotSpan.size(); ++i) {
                    if (slotSpan[i] != std::byte{0}) {
                        isEmptySlot = false;
                        break;
                    }
                }

                if (!isEmptySlot) {
                    // Decrypt and create Pokemon9LZA object
                    boxes[boxIndex][slot] = std::make_unique<Pokemon9LZA>(slotSpan);
                } else {
                    // Empty slot
                    boxes[boxIndex][slot] = nullptr;
                }
            }
        }
    }

    void Trainer9LZA::parseBoxLayoutBlock(const Block& block)
    {
        /**
         * BOX_LAYOUT Block Structure:
         * Box names stored sequentially:
         * - Box 0 name: offset 0 (34 bytes, UTF-16LE)
         * - Box 1 name: offset 34
         * - ... for all 32 boxes
         *
         * Each name is BOX_NAME_LENGTH_Gen9 bytes (34 bytes).
         */
        for (size_t boxIndex = 0; boxIndex < BOX_COUNT9_LZA; ++boxIndex) {
            size_t offset = boxIndex * BOX_NAME_LENGTH9_LZA;
            if (offset + BOX_NAME_LENGTH9_LZA <= block.data.size()) {
                // Extract box name (UTF-16LE string)
                std::u16string boxNameU16 = getString(
                    block.data.data() + offset,
                    BOX_NAME_LENGTH9_LZA
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

    void Trainer9LZA::parseCurrentBoxBlock(const Block& block)
    {
        // "U32 Box Index" -- a scalar block; PKHeX reads it as a byte (0..31 fits one byte).
        if (block.data.empty()) return;
        uint8_t box = block.data[0];
        if (box < BOX_COUNT9_LZA) this->currentBox = box;
    }

    void Trainer9LZA::updateCurrentBoxBlock()
    {
        // Inverse of parseCurrentBoxBlock: write the low byte and clear the rest of the scalar
        // (PKHeX stores it via SetValue<byte>, so the upper bytes are 0).
        for (auto& block : blocks) {
            if (block.key != CURRENT_BOX9_LZA || block.data.empty()) continue;
            block.data[0] = static_cast<uint8_t>(currentBox);
            for (size_t i = 1; i < block.data.size() && i < 4; ++i) block.data[i] = 0;
            break;
        }
    }

    void Trainer9LZA::parseSaveRevisionBlock(const Block& block)
    {
        /**
         * SAVE_REVISION Block Structure (Pokemon Legends Z-A):
         * Contains a u64 (8 bytes) value indicating the save revision:
         * - 0: Base game (version 1.0.x)
         * - 1: Mega Dimension DLC (version 2.0.0+)
         *
         * This value is critical for determining:
         * - Which Pokemon species are available
         * - Which moves are legal
         * - Which items exist
         * - Block key compatibility
         */
        if (block.data.size() < 8) {
            // Default to base game if block is missing or too small
            this->saveRevision = 0;
            this->saveRevisionString = "Base";
            this->gameVersionString = "v1.0";
            logInfoToFile("SAVE_REVISION block too small, defaulting to Base");
            return;
        }

        // Read the u64 revision value
        uint64_t revision = readUInt64LittleEndian(block.data.data());
        this->saveRevision = static_cast<int>(revision);

        // Map revision to human-readable string and version
        switch (this->saveRevision) {
            case 0:
                this->saveRevisionString = "Base";
                this->gameVersionString = "v1.0";  // Base game v1.0.x
                break;
            case 1:
                this->saveRevisionString = "Mega Dimension";
                this->gameVersionString = "v2.0";  // Mega Dimension DLC requires v2.0.0+
                break;
            default:
                // Future DLC/updates
                this->saveRevisionString = "Rev " + std::to_string(this->saveRevision);
                this->gameVersionString = "v" + std::to_string(this->saveRevision + 1) + ".0";
                break;
        }

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Detected save revision: %d (%s, %s)",
            this->saveRevision, this->saveRevisionString.c_str(), this->gameVersionString.c_str());
        logInfoToFile(buffer);
    }

    // ========================================
    // Block Update Methods
    // ========================================

    void Trainer9LZA::updatePartyBlock()
    {
        /**
         * Updates the PARTY block with modified Pokemon data.
         *
         * Process:
         * 1. Find the PARTY block
         * 2. Ensure block is large enough (6 slots * PARTY_SLOT_SIZE9_LZA)
         * 3. For each party Pokemon:
         *    a. Get encryption constant from Pokemon data
         *    b. Encrypt Pokemon data using encryptArray9
         *    c. Write encrypted data to block at correct offset (with gaps)
         * 4. Zero out empty slots
         */
        for (auto& block : blocks) {
            if (block.key == PARTY9_LZA) {
                // Ensure the block data is large enough (including gaps)
                size_t requiredSize = MAX_PARTY_SLOTS * m_partySlotStride;
                if (block.data.size() < requiredSize) {
                    block.data.resize(requiredSize, 0);
                }

                // Write each party Pokemon
                for (size_t i = 0; i < party.size() && i < MAX_PARTY_SLOTS; ++i) {
                    // Calculate offset (packed for S/V, gapped for Z-A)
                    const size_t offset = i * m_partySlotStride;

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
                        std::byte* encryptedData = encryptArray9LZA(decryptedSpan, ec);

                        // Write encrypted data to block (only SIZE_PARTY9_LZA bytes)
                        std::memcpy(&block.data[offset], encryptedData, pokemon->getDataSize());

                        // Zero out the gap after the Pokemon data (0 bytes for S/V's packed layout)
                        std::memset(&block.data[offset + SIZE_PARTY9_LZA], 0, m_slotGapZero);

                        // Clean up encrypted buffer
                        delete[] encryptedData;
                    } else {
                        // Empty slot - write zeros for entire slot (data + gap)
                        std::memset(&block.data[offset], 0, m_partySlotStride);
                    }
                }

                // Zero out any remaining slots
                for (size_t i = party.size(); i < MAX_PARTY_SLOTS; ++i) {
                    const size_t offset = i * m_partySlotStride;
                    std::memset(&block.data[offset], 0, m_partySlotStride);
                }

                break;
            }
        }
    }

    void Trainer9LZA::updateBoxNameBlock()
    {
        // Inverse of parseBoxLayoutBlock. See Trainer8SWSH::updateBoxNameBlock for why the block is
        // bounds-checked rather than resized.
        for (auto& block : blocks) {
            if (block.key != BOX_LAYOUT9_LZA) continue;
            for (size_t boxIndex = 0; boxIndex < BOX_COUNT9_LZA && boxIndex < boxNames.size(); ++boxIndex) {
                if (!isBoxNameDirty(boxIndex)) continue;   // never persist a display default
                const size_t offset = boxIndex * BOX_NAME_LENGTH9_LZA;
                if (offset + BOX_NAME_LENGTH9_LZA > block.data.size()) break;
                setString(block.data.data() + offset, BOX_NAME_LENGTH9_LZA,
                          utf8ToUtf16(boxNames[boxIndex]), BOX_NAME_LENGTH9_LZA / 2 - 1);
            }
            break;
        }
    }

    void Trainer9LZA::updateBoxBlock()
    {
        /**
         * Updates the BOX block with modified Pokemon data.
         *
         * Process similar to updatePartyBlock, but for all boxes:
         * 1. Find the BOX block
         * 2. Ensure block is large enough (32 boxes * 30 slots * BOX_SLOT_SIZE9_LZA)
         * 3. For each box and slot:
         *    a. If Pokemon exists, encrypt and write
         *    b. If slot is empty, write zeros
         */
        for (auto& block : blocks) {
            if (block.key == BOX9_LZA) {
                // Ensure the block data is large enough for all boxes (including gaps)
                size_t requiredSize = BOX_COUNT9_LZA * BOX_SLOTS * m_boxSlotStride;
                if (block.data.size() < requiredSize) {
                    block.data.resize(requiredSize, 0);
                }

                // Empty box slots in the real save are an ENCRYPTED blank that DECRYPTS to species 0 —
                // NOT literal zeros. The game decrypts every box slot and checks species; a zeroed slot
                // decrypts to garbage and renders a BAD EGG. So reuse the game's own blank: copy the raw
                // bytes of an existing empty (species-0) slot. If every box is full (no blank to copy),
                // synthesize one from an all-zero PK9 encrypted with EC 0 (decrypts back to species 0).
                std::vector<uint8_t> blankSlot;
                for (size_t bi = 0; bi < BOX_COUNT9_LZA && blankSlot.empty(); ++bi) {
                    for (size_t s = 0; s < BOX_SLOTS; ++s) {
                        if (boxes[bi][s] && boxes[bi][s]->speciesID() == 0) {
                            const size_t off = (bi * BOX_SLOTS + s) * m_boxSlotStride;
                            if (off + m_boxSlotStride <= block.data.size()) {
                                blankSlot.assign(block.data.begin() + off,
                                                 block.data.begin() + off + m_boxSlotStride);
                                break;
                            }
                        }
                    }
                }
                if (blankSlot.empty()) {
                    std::vector<std::byte> zero(SIZE_PARTY9_LZA, std::byte{0});
                    std::byte* enc = encryptArray9LZA(
                        std::span<const std::byte>(zero.data(), SIZE_PARTY9_LZA), 0);
                    blankSlot.assign(reinterpret_cast<const uint8_t*>(enc),
                                     reinterpret_cast<const uint8_t*>(enc) + SIZE_PARTY9_LZA);
                    blankSlot.resize(m_boxSlotStride, 0);  // pad the Z-A slot gap with zeros
                    delete[] enc;
                }

                // Write each Pokemon back to the block
                for (size_t boxIndex = 0; boxIndex < BOX_COUNT9_LZA; ++boxIndex) {
                    for (size_t slot = 0; slot < BOX_SLOTS; ++slot) {
                        // Calculate offset (packed for S/V, gapped for Z-A)
                        const size_t offset = (boxIndex * BOX_SLOTS + slot) * m_boxSlotStride;

                        // Gate on species, not just the pointer (matches updatePartyBlock). A slot left
                        // holding a non-null but blank/species-0 mon (e.g. after a bank move) must be
                        // zeroed to read as EMPTY in-game — re-encrypting a blank writes a bad egg.
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
                            std::byte* encryptedData = encryptArray9LZA(decryptedSpan, ec);

                            // Write encrypted data to block (only SIZE_PARTY9_LZA bytes)
                            std::memcpy(&block.data[offset], encryptedData, pokemon->getDataSize());

                            // Zero out the gap after the Pokemon data (0 bytes for S/V's packed layout)
                            std::memset(&block.data[offset + SIZE_PARTY9_LZA], 0, m_slotGapZero);

                            // Clean up encrypted buffer
                            delete[] encryptedData;
                        } else {
                            // Empty/cleared slot: write the game's encrypted blank, NOT zeros. Zeros
                            // decrypt to garbage in-game and show as a BAD EGG in every empty slot.
                            std::memcpy(&block.data[offset], blankSlot.data(), m_boxSlotStride);
                        }
                    }
                }
                break;
            }
        }
    }

    std::unique_ptr<::Pokemon::Pokemon> Trainer9LZA::createBlankPokemon() const
    {
        // Mirror updateBoxBlock()'s encrypted-blank fallback: a zeroed *decrypted* PK9 buffer
        // encrypted with EC/seed 0, then fed to the ctor (which decrypts it straight back to zeros)
        // -> a clean species-0, Sanity-0, checksum-valid entity. Raw zeros in the ctor would decrypt
        // to garbage (BAD EGG); the encrypt->decrypt round-trip is what makes the blank valid.
        std::vector<std::byte> zero(SIZE_PARTY9_LZA, std::byte{0});
        std::byte* enc = encryptArray9LZA(
            std::span<const std::byte>(zero.data(), SIZE_PARTY9_LZA), 0);
        auto p = std::make_unique<Pokemon9LZA>(
            std::span<const std::byte>(enc, SIZE_PARTY9_LZA));
        delete[] enc;
        return p;
    }

    void Trainer9LZA::updateItemBlock()
    {
        /**
         * Updates the ITEM block with modified inventory data.
         *
         * Process:
         * 1. Find the ITEM block
         * 2. Ensure block is large enough (0xBB80 bytes)
         * 3. Write items to their indexed positions (itemID * 0x10)
         *
         * Gen 9 stores items BY ITEM ID as index
         * Block size: 0xBB80 bytes (47,872 bytes)
         * Item size: 0x10 bytes (16 bytes)
         */

        for (auto& block : blocks) {
            if (block.key != ITEM9_LZA) continue;

            // Size-preserving in-place write. Gen 9 stores every item at a fixed index (itemId * 0x10)
            // as {pouchId@0x00, count@0x04 (int32 LE), flags@0x08 (isNew=bit0, isFavorite=bit1)}.
            //
            // PKSE places each item in a pouch by legal-list membership (getPouchItems, mirroring
            // PKHeX's spans); the GAME instead keys the bag off the pouchId in each record. A freshly
            // CREATED item (add or change-type) lands in a never-held slot whose pouchId is
            // the "none" sentinel, so the game hides it even with the count set -- exactly the "Canari
            // Bread shows in PKSE but not in-game" report. We stamp the pouch's canonical pouchId
            // (PKHeX InventoryItem9a.Pouch*) on every present item -- a no-op for items the game already
            // had, the fix for new ones. Index by PouchType9LZA. (never resize/rebuild.)
            static const uint32_t POUCH_ID9_LZA[POUCH_COUNT9_LZA] = { 0, 1, 5, 2, 6, 7, 3, 4 };
            const size_t blockSize = block.data.size();
            for (int i = 0; i < static_cast<int>(POUCH_COUNT9_LZA); i++) {
                for (const auto& item : items[i]) {
                    const size_t offset = static_cast<size_t>(item.itemId) * ITEM_SIZE9_LZA;
                    if (offset + ITEM_SIZE9_LZA > blockSize) continue;

                    const int32_t count = static_cast<int32_t>(item.count);
                    block.data[offset + 4] = static_cast<uint8_t>(count & 0xFF);
                    block.data[offset + 5] = static_cast<uint8_t>((count >> 8) & 0xFF);
                    block.data[offset + 6] = static_cast<uint8_t>((count >> 16) & 0xFF);
                    block.data[offset + 7] = static_cast<uint8_t>((count >> 24) & 0xFF);

                    if (item.count > 0) {
                        const uint32_t pid = POUCH_ID9_LZA[i];
                        block.data[offset + 0] = static_cast<uint8_t>(pid & 0xFF);
                        block.data[offset + 1] = static_cast<uint8_t>((pid >> 8) & 0xFF);
                        block.data[offset + 2] = static_cast<uint8_t>((pid >> 16) & 0xFF);
                        block.data[offset + 3] = static_cast<uint8_t>((pid >> 24) & 0xFF);
                        // isNew = flags bit 0. Only SET (freshly-added items); never clear, so the
                        // game's own "new" markers on existing items survive round-trips.
                        if (item.isNew) block.data[offset + 8] = static_cast<uint8_t>(block.data[offset + 8] | 0x01);
                    }
                }
            }
            break;
        }
    }
}
