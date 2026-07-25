/**
 * Trainer7LGPE.cpp - Generation 7 Let's Go Trainer Implementation
 *
 * This file implements the Trainer7LGPE class for Pokemon Let's Go Pikachu/Eevee save files.
 * Handles Gen 7-specific block parsing, Pokemon encryption/decryption, and
 * save file serialization.
 */
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "Trainer/Trainer7LGPE.h"
#include "Pokemon/Pokemon7LGPE.h"
#include "Utils/Logger.h"

using namespace Utils;
using namespace Pokemon;

namespace Trainer {
    // ========================================
    // Block Parsing Methods
    // ========================================

    void Trainer7LGPE::parseBlock(const Block& block)
    {
        switch (block.key) {
            case MY_ITEM7_LGPE:
                parseMyItemBlock(block);
                break;
            case MY_STATUS7_LGPE:
                parseMyStatusBlock(block);
                break;
            case POKE_LIST_HEADER7_LGPE:
                parsePokeListHeaderBlock(block);
                break;
            case POKE_LIST_POKEMON7_LGPE:
                parsePokeListPokemonBlock(block);
                break;
            case MISC7_LGPE:
                parseMiscBlock(block);
                break;
            case PLAY_TIME7_LGPE:
                parsePlayTimeBlock(block);
                break;
            // Additional blocks can be handled here
            default:
                // Unknown block - skip
                break;
        }
    }

    void Trainer7LGPE::parseMyStatusBlock(const Block& block)
    {
        /**
         * MY_STATUS Block Structure (from PKHeX MyStatus7):
         * 0x00: ID32 (4 bytes) - Combined TID16 and SID16
         * 0x04: Game version (1 byte)
         * 0x05: Gender (1 byte, 0=Male, 1=Female)
         * 0x38: Trainer Name (26 bytes, UTF-16LE)
         *
         * ID32 format: SID16 << 16 | TID16
         * Display TID: ID32 % 1000000
         * Display SID: ID32 / 1000000
         */
        char logBuffer[256];
        snprintf(logBuffer, sizeof(logBuffer), "parseMyStatusBlock: block size = %zu bytes", block.data.size());
        logInfoToFile(logBuffer);

        if (block.data.size() < 0x38 + 26) {
            logInfoToFile("Insufficient data in MY_STATUS block");
            return;
        }

        // Parse trainer ID (at start of block)
        this->ID32 = readUInt32LittleEndian(&block.data[0x00]);
        this->TID16 = readUInt16LittleEndian(&block.data[0x00]);
        this->SID16 = readUInt16LittleEndian(&block.data[0x02]);
        this->TID = this->ID32 % 1000000;
        this->SID = this->ID32 / 1000000;

        // Parse trainer name (UTF-16LE string at offset 0x38)
        this->trainerName = utf16ToUtf8(getString(&block.data[0x38], 26));
        this->trainerGender = block.data[0x05] & 1;   // 0x05: gender (0=M, 1=F)

        // Log parsed values for debugging
        snprintf(logBuffer, sizeof(logBuffer), "Parsed trainer: Name='%s', ID32=%u, TID=%u, SID=%u",
                 this->trainerName.c_str(), this->ID32, this->TID, this->SID);
        logInfoToFile(logBuffer);

        // Let's Go doesn't have save revision/DLC, set base values
        this->saveRevision = 0;
        this->saveRevisionString = "Base";
        this->gameVersionString = "";
    }

    void Trainer7LGPE::parsePokeListHeaderBlock(const Block& block)
    {
        /**
         * POKE_LIST_HEADER Block Structure (from PKHeX PokeListHeader.cs):
         *
         * Let's Go uses an INDEX-BASED party system.
         * IMPORTANT: There is NO explicit party count field!
         *
         * Structure (16 bytes total):
         * 0x00-0x01: Party Slot 0 Pointer (u16)
         * 0x02-0x03: Party Slot 1 Pointer (u16)
         * 0x04-0x05: Party Slot 2 Pointer (u16)
         * 0x06-0x07: Party Slot 3 Pointer (u16)
         * 0x08-0x09: Party Slot 4 Pointer (u16)
         * 0x0A-0x0B: Party Slot 5 Pointer (u16)
         * 0x0C-0x0D: Starter Pointer (u16)
         * 0x0E-0x0F: List Count / Next Empty Slot (u16)
         *
         * MAX_SLOTS = 1000 (valid indices: 0-999)
         * SLOT_EMPTY = 1001 marks empty party positions
         *
         * Party count is CALCULATED by counting non-1001 party pointers.
         */
        char logBuffer[256];
        snprintf(logBuffer, sizeof(logBuffer), "parsePokeListHeaderBlock: block size = %zu bytes", block.data.size());
        logInfoToFile(logBuffer);

        if (block.data.size() < 16) {
            logInfoToFile("Insufficient data in POKE_LIST_HEADER block");
            return;
        }

        // Debug: Log first 16 bytes
        snprintf(logBuffer, sizeof(logBuffer), "Header bytes 0-15: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 block.data[0], block.data[1], block.data[2], block.data[3],
                 block.data[4], block.data[5], block.data[6], block.data[7],
                 block.data[8], block.data[9], block.data[10], block.data[11],
                 block.data[12], block.data[13], block.data[14], block.data[15]);
        logInfoToFile(logBuffer);

        constexpr uint16_t SLOT_EMPTY = 1001;
        constexpr uint16_t MAX_SLOTS = 1000;

        // Read party pointers (6 × u16 starting at offset 0)
        // Party count is calculated, not stored!
        uint8_t calculatedPartyCount = 0;
        for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) {
            uint16_t index = readUInt16LittleEndian(&block.data[i * 2]);
            partyIndices[i] = index;

            // Count valid (non-empty) party members
            if (index != SLOT_EMPTY && index < MAX_SLOTS) {
                calculatedPartyCount++;
            }

            snprintf(logBuffer, sizeof(logBuffer), "Party slot %zu: index = %u%s",
                     i, index, (index == SLOT_EMPTY) ? " (empty)" : (index >= MAX_SLOTS ? " (INVALID)" : ""));
            logInfoToFile(logBuffer);
        }

        this->partyCount = calculatedPartyCount;
        snprintf(logBuffer, sizeof(logBuffer), "Calculated party count: %u", this->partyCount);
        logInfoToFile(logBuffer);

        // Read starter index (u16 at offset 0x0C)
        starterIndex = readUInt16LittleEndian(&block.data[0x0C]);
        snprintf(logBuffer, sizeof(logBuffer), "Starter index: %u%s",
                 starterIndex, (starterIndex == SLOT_EMPTY) ? " (empty)" : "");
        logInfoToFile(logBuffer);

        // Read list count / next empty slot (u16 at offset 0x0E)
        uint16_t listCount = readUInt16LittleEndian(&block.data[0x0E]);
        snprintf(logBuffer, sizeof(logBuffer), "List count (next empty slot): %u", listCount);
        logInfoToFile(logBuffer);
    }

    void Trainer7LGPE::parsePokeListPokemonBlock(const Block& block)
    {
        /**
         * POKE_LIST_POKEMON Block Structure:
         *
         * In Let's Go, ALL Pokemon (party + boxes) are stored together:
         * - Indices 0-999: Box slots (40 boxes × 25 slots)
         * - Party members are identified by partyIndices from PokeListHeader
         *
         * Each slot is SIZE_PARTY7_LGPE bytes (260 bytes).
         * Empty slots are zeroed out.
         * Total: 1000 * 260 = 260,000 bytes (0x3F7A0)
         */
        char logBuffer[256];
        snprintf(logBuffer, sizeof(logBuffer), "parsePokeListPokemonBlock: block size = %zu bytes", block.data.size());
        logInfoToFile(logBuffer);

        const std::span<const std::byte> blockSpan(
            reinterpret_cast<const std::byte*>(block.data.data()),
            block.data.size()
        );

        size_t pokemonCount = 0;
        constexpr uint16_t SLOT_EMPTY = 1001;
        constexpr size_t MAX_STORAGE_SLOTS = BOX_COUNT7_LGPE * SLOTS_PER_BOX7_LGPE; // 1000

        // Parse all Pokemon storage (boxes)
        for (size_t boxIndex = 0; boxIndex < BOX_COUNT7_LGPE; ++boxIndex) {
            // Initialize box name if not already set
            if (boxNames[boxIndex].empty()) {
                boxNames[boxIndex] = "盒子 " + std::to_string(boxIndex + 1);
            }

            for (size_t slot = 0; slot < SLOTS_PER_BOX7_LGPE; ++slot) {
                // Calculate offset: (boxIndex * slots per box + slot) * bytes per pokemon
                const size_t offset = (boxIndex * SLOTS_PER_BOX7_LGPE + slot) * SIZE_PARTY7_LGPE;
                if (offset + SIZE_PARTY7_LGPE > block.data.size()) {
                    snprintf(logBuffer, sizeof(logBuffer), "Box %zu slot %zu: offset %zu exceeds block size", boxIndex, slot, offset);
                    logInfoToFile(logBuffer);
                    break;
                }

                std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_PARTY7_LGPE);

                // Check if slot has valid Pokemon data (non-zero encryption constant)
                uint32_t ec = readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(slotSpan.data()));
                if (ec != 0) {
                    boxes[boxIndex][slot] = std::make_unique<Pokemon7LGPE>(slotSpan);
                    pokemonCount++;
                } else {
                    boxes[boxIndex][slot] = nullptr;
                }
            }
        }

        snprintf(logBuffer, sizeof(logBuffer), "Loaded %zu box Pokemon", pokemonCount);
        logInfoToFile(logBuffer);

        // Now populate party from partyIndices
        // Party indices point to storage slots (0-999)
        for (size_t i = 0; i < partyCount && i < MAX_PARTY_SLOTS; ++i) {
            uint16_t storageIndex = partyIndices[i];

            if (storageIndex == SLOT_EMPTY || storageIndex >= MAX_STORAGE_SLOTS) {
                snprintf(logBuffer, sizeof(logBuffer), "Party slot %zu: skipped (index=%u)", i, storageIndex);
                logInfoToFile(logBuffer);
                continue;
            }

            // Convert flat index to box/slot
            size_t boxIndex = storageIndex / SLOTS_PER_BOX7_LGPE;
            size_t slotIndex = storageIndex % SLOTS_PER_BOX7_LGPE;

            if (boxIndex < BOX_COUNT7_LGPE && boxes[boxIndex][slotIndex]) {
                // Read Pokemon data directly from block for party (to create independent copy)
                const size_t offset = storageIndex * SIZE_PARTY7_LGPE;
                if (offset + SIZE_PARTY7_LGPE <= block.data.size()) {
                    std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_PARTY7_LGPE);
                    party.push_back(std::make_unique<Pokemon7LGPE>(slotSpan));
                    snprintf(logBuffer, sizeof(logBuffer), "Party slot %zu: loaded from storage index %u (box %zu, slot %zu)",
                             i, storageIndex, boxIndex, slotIndex);
                    logInfoToFile(logBuffer);
                }
            } else {
                snprintf(logBuffer, sizeof(logBuffer), "Party slot %zu: storage index %u has no Pokemon", i, storageIndex);
                logInfoToFile(logBuffer);
            }
        }

        snprintf(logBuffer, sizeof(logBuffer), "Loaded %zu party Pokemon from indices", party.size());
        logInfoToFile(logBuffer);
    }

    void Trainer7LGPE::parseMiscBlock(const Block& block)
    {
        /**
         * MISC Block Structure:
         * 0x04: Money (4 bytes) - Trainer's currency amount
         */
        if (block.data.size() < 0x04 + 4) {
            return;
        }

        this->money = readUInt32LittleEndian(&block.data[0x04]);
    }

    void Trainer7LGPE::parsePlayTimeBlock(const Block& block)
    {
        /**
         * PLAY_TIME Block Structure:
         * 0x00: Hours (2 bytes)
         * 0x02: Minutes (1 byte)
         * 0x03: Seconds (1 byte)
         */
        if (block.data.size() < 4) {
            return;
        }

        uint16_t hours = readUInt16LittleEndian(&block.data[0x00]);
        uint8_t minutes = block.data[0x02];
        uint8_t seconds = block.data[0x03];

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Play time: %d:%02d:%02d", hours, minutes, seconds);
        logInfoToFile(buffer);
    }

    void Trainer7LGPE::parseMyItemBlock(const Block& block)
    {
        /**
         * MY_ITEM Block Structure for Let's Go:
         * Items are stored in pouches (categories):
         * - Medicine: Healing items
         * - TMs: Technical Machines
         * - Candy: Stat-boosting candies
         * - PowerUp: Evolution stones, PP items
         * - Catching: Poke Balls, Berries
         * - Battle: Battle items, Mega Stones
         * - KeyItems: Quest items
         *
         * Each item is stored as 4 bytes: (count << 10) | itemId
         */
        char logBuffer[256];
        snprintf(logBuffer, sizeof(logBuffer), "parseMyItemBlock: block size = %zu bytes", block.data.size());
        logInfoToFile(logBuffer);

        // Initialize items vector with pouches for each type
        items.resize(POUCH_COUNT7_LGPE);

        // Load each pouch
        for (size_t p = 0; p < POUCH_COUNT7_LGPE; p++) {
            PouchType7LGPE pouchType = static_cast<PouchType7LGPE>(p);
            const PouchInfo7LGPE& info = getPouchInfo7LGPE(pouchType);

            std::vector<InventoryItem> pouch;
            pouch.reserve(info.maxSlots);

            // Read items from block data
            for (int i = 0; i < info.maxSlots; i++) {
                size_t offset = info.offset + (i * 4);
                if (offset + 4 <= block.data.size()) {
                    uint32_t itemValue = readUInt32LittleEndian(&block.data[offset]);
                    InventoryItem7LGPE item = InventoryItem7LGPE::fromValue(itemValue);

                    // Only add items with valid IDs (non-zero). isNew (bit 30) is preserved as-read
                    // so it round-trips faithfully on save.
                    if (item.itemId != 0) {
                        pouch.push_back(item);
                    }
                }
            }

            items[p] = std::move(pouch);
        }

        // Log item counts
        size_t totalItems = 0;
        for (const auto& pouch : items) {
            totalItems += pouch.size();
        }
        snprintf(logBuffer, sizeof(logBuffer), "Loaded %zu total items across %zu pouches", totalItems, items.size());
        logInfoToFile(logBuffer);
    }

    // ========================================
    // Block Update Methods
    // ========================================

    namespace {
        // Encrypt a Pokemon's decrypted buffer (seed = EncryptionConstant at 0x00) and write
        // it into `dst` at `offset`. Inverse of the read path's decryptArray7LGPE.
        void writeEncryptedPokemon(std::vector<uint8_t>& dst, size_t offset, const ::Pokemon::Pokemon& pokemon)
        {
            const size_t size = pokemon.getDataSize();
            if (offset + size > dst.size()) return;
            uint32_t ec = readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(pokemon.getData().data()));
            std::span<const std::byte> decrypted(pokemon.getData().data(), size);
            std::byte* enc = Encryption::encryptArray7LGPE(decrypted, ec);
            std::memcpy(&dst[offset], enc, size);
            delete[] enc;
        }
    }

    void Trainer7LGPE::updatePartyBlock()
    {
        // No-op for LGPE: party, boxes, and the header are all serialized together in
        // updateBoxBlock(), which compacts the gapless storage and remaps the party/starter
        // pointers in one pass. (saveTrainerInfoLetsGo calls updateBoxBlock() first.)
    }

    void Trainer7LGPE::updateBoxBlock()
    {
        /**
         * Serializes ALL LGPE storage on save (updatePartyBlock is a no-op that defers here).
         * LGPE storage is a GAPLESS packed list of 1000 slots, so moves that leave gaps must be
         * compacted before writing:
         *   1. Walk boxes in order, collect occupied Pokemon into a packed list, and record each
         *      old storage index -> new packed index (this removes any gaps left by moves).
         *   2. Write the packed list to storage; zero the tail.
         *   3. Overlay the party copies at their remapped indices (party edits win over the box
         *      copy of the same slot).
         *   4. Write the header: remapped party pointers + starter + the packed count.
         * For in-place edits and swaps (no gaps) this collapses to the identity mapping, so it
         * produces exactly the previous result.
         */
        constexpr size_t TOTAL = BOX_COUNT7_LGPE * SLOTS_PER_BOX7_LGPE; // 1000
        constexpr uint16_t SLOT_EMPTY = 1001;

        // 1. Build packed list + old->new index map.
        std::vector<int> oldToNew(TOTAL, -1);
        std::vector<::Pokemon::Pokemon*> packed;
        packed.reserve(TOTAL);
        for (size_t box = 0; box < BOX_COUNT7_LGPE; ++box) {
            for (size_t slot = 0; slot < SLOTS_PER_BOX7_LGPE; ++slot) {
                const size_t oldIdx = box * SLOTS_PER_BOX7_LGPE + slot;
                if (boxes[box][slot] && boxes[box][slot]->speciesID() != 0) {
                    oldToNew[oldIdx] = static_cast<int>(packed.size());
                    packed.push_back(boxes[box][slot].get());
                }
            }
        }
        const uint16_t packedCount = static_cast<uint16_t>(packed.size());

        auto remap = [&](uint16_t oldIdx) -> uint16_t {
            return (oldIdx < TOTAL && oldToNew[oldIdx] >= 0) ? static_cast<uint16_t>(oldToNew[oldIdx]) : SLOT_EMPTY;
        };

        // 2 & 3. Write packed storage, then overlay party copies at their remapped indices.
        for (auto& block : blocks) {
            if (block.key != POKE_LIST_POKEMON7_LGPE) continue;
            const size_t required = TOTAL * SIZE_PARTY7_LGPE;
            if (block.data.size() < required) block.data.resize(required, 0);

            for (size_t n = 0; n < TOTAL; ++n) {
                const size_t offset = n * SIZE_PARTY7_LGPE;
                if (n < packed.size()) writeEncryptedPokemon(block.data, offset, *packed[n]);
                else std::memset(&block.data[offset], 0, SIZE_PARTY7_LGPE);
            }

            for (size_t i = 0; i < party.size() && i < MAX_PARTY_SLOTS; ++i) {
                const uint16_t newIdx = remap(partyIndices[i]);
                if (newIdx >= TOTAL) continue;
                if (!party[i] || party[i]->speciesID() == 0) continue;
                writeEncryptedPokemon(block.data, static_cast<size_t>(newIdx) * SIZE_PARTY7_LGPE, *party[i]);
            }
            break;
        }

        // 4. Header: remapped party pointers + starter + packed count.
        for (auto& block : blocks) {
            if (block.key != POKE_LIST_HEADER7_LGPE) continue;
            if (block.data.size() >= 0x10) {
                for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) {
                    writeUInt16LittleEndian(&block.data[i * 2], remap(partyIndices[i]));
                }
                writeUInt16LittleEndian(&block.data[0x0C], remap(starterIndex));
                writeUInt16LittleEndian(&block.data[0x0E], packedCount);
            }
            break;
        }
    }

    bool Trainer7LGPE::compactStorage()
    {
        /**
         * The in-memory twin of updateBoxBlock()'s step 1. LGPE storage is one gapless 1000-slot
         * list, so a move that vacates a slot leaves a hole the game cannot represent; the save
         * path already closes it, and this closes it live so the editor shows what will be written
         * instead of a gap that silently disappears on reload.
         *
         * The dangerous half is the remap at the end, NOT the shuffle: party members and the
         * partner reference storage BY INDEX, so re-packing without remapping would leave them
         * pointing at whichever Pokemon slid into the vacated slot.
         */
        constexpr size_t TOTAL = BOX_COUNT7_LGPE * SLOTS_PER_BOX7_LGPE;   // 1000
        constexpr uint16_t SLOT_EMPTY = 1001;

        // Cheap pre-scan, no allocation: storage is packed iff no occupied slot follows an empty
        // one. This runs every frame, so the common "nothing to do" case must stay allocation-free.
        bool seenEmpty = false, hasGap = false;
        for (size_t box = 0; box < BOX_COUNT7_LGPE && !hasGap; ++box) {
            for (size_t slot = 0; slot < SLOTS_PER_BOX7_LGPE; ++slot) {
                const auto& cell = boxes[box][slot];
                // Gate on species, not the pointer: a "ghost" (non-null but species 0) is an empty
                // slot, and treating it as occupied would hold the hole open forever.
                if (!cell || cell->speciesID() == 0) seenEmpty = true;
                else if (seenEmpty) { hasGap = true; break; }
            }
        }
        if (!hasGap) return false;

        std::vector<int> oldToNew(TOTAL, -1);
        std::vector<std::unique_ptr<::Pokemon::Pokemon>> packed;
        packed.reserve(TOTAL);
        for (size_t box = 0; box < BOX_COUNT7_LGPE; ++box) {
            for (size_t slot = 0; slot < SLOTS_PER_BOX7_LGPE; ++slot) {
                auto& cell = boxes[box][slot];
                if (cell && cell->speciesID() != 0) {
                    oldToNew[box * SLOTS_PER_BOX7_LGPE + slot] = static_cast<int>(packed.size());
                    packed.push_back(std::move(cell));
                } else {
                    cell.reset();       // drop ghosts while we are here
                }
            }
        }

        size_t n = 0;
        for (size_t box = 0; box < BOX_COUNT7_LGPE; ++box) {
            for (size_t slot = 0; slot < SLOTS_PER_BOX7_LGPE; ++slot) {
                boxes[box][slot] = (n < packed.size()) ? std::move(packed[n++]) : nullptr;
            }
        }

        // Remap everything that points INTO storage. Same mapping updateBoxBlock() will apply.
        auto remap = [&](uint16_t oldIdx) -> uint16_t {
            return (oldIdx < TOTAL && oldToNew[oldIdx] >= 0)
                 ? static_cast<uint16_t>(oldToNew[oldIdx]) : SLOT_EMPTY;
        };
        for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) partyIndices[i] = remap(partyIndices[i]);
        starterIndex = remap(starterIndex);
        return true;
    }

    void Trainer7LGPE::mirrorPartyMemberFromBox(size_t boxIndex, size_t slotIndex)
    {
        // If this box slot is a party member, copy its (just-edited) bytes into the party copy so
        // updateBoxBlock()'s party overlay doesn't clobber the edit on save. Both are PK7b (same
        // size), and the box edit already recomputed stats + checksum, so a raw buffer copy suffices.
        const int pos = getPartyPosition(boxIndex, slotIndex);  // 1-based; 0 if not a party member
        if (pos <= 0) return;
        const size_t i = static_cast<size_t>(pos - 1);
        if (i >= party.size() || !party[i]) return;
        if (boxIndex >= boxes.size() || slotIndex >= boxes[boxIndex].size()) return;
        auto& src = boxes[boxIndex][slotIndex];
        if (!src) return;
        const size_t n = std::min(party[i]->getDataSize(), src->getDataSize());
        std::memcpy(party[i]->getData().data(), src->getData().data(), n);
    }

    void Trainer7LGPE::mirrorPartyMemberFromParty(size_t partyIndex)
    {
        // Reverse direction: push a party-copy edit back into its box/storage slot (the display copy)
        // so the two representations stay byte-identical.
        constexpr uint16_t SLOT_EMPTY = 1001;
        if (partyIndex >= party.size() || !party[partyIndex]) return;
        const uint16_t idx = partyIndices[partyIndex];
        if (idx == SLOT_EMPTY) return;
        const size_t boxIndex = idx / SLOTS_PER_BOX7_LGPE;
        const size_t slotIndex = idx % SLOTS_PER_BOX7_LGPE;
        if (boxIndex >= boxes.size() || slotIndex >= boxes[boxIndex].size()) return;
        auto& dst = boxes[boxIndex][slotIndex];
        if (!dst) return;
        const size_t n = std::min(dst->getDataSize(), party[partyIndex]->getDataSize());
        std::memcpy(dst->getData().data(), party[partyIndex]->getData().data(), n);
    }

    std::unique_ptr<::Pokemon::Pokemon> Trainer7LGPE::createBlankPokemon() const
    {
        // A zeroed *decrypted* PB7 buffer encrypted with EC/seed 0, then fed to the ctor (which
        // decrypts it straight back to zeros) -> a clean species-0, checksum-valid entity. LGPE's
        // own empty box slots are written as raw zeros (the read path gates on a non-zero EC), but a
        // *live* blank entity still needs a valid decrypted PB7 buffer, so use the encrypt->decrypt
        // round-trip here (same principle as the SwishCrypto gens' encrypted-blank fallback).
        std::vector<std::byte> zero(SIZE_PARTY7_LGPE, std::byte{0});
        std::byte* enc = Encryption::encryptArray7LGPE(
            std::span<const std::byte>(zero.data(), SIZE_PARTY7_LGPE), 0);
        auto p = std::make_unique<Pokemon7LGPE>(
            std::span<const std::byte>(enc, SIZE_PARTY7_LGPE));
        delete[] enc;
        return p;
    }

    void Trainer7LGPE::updateItemBlock()
    {
        /**
         * Serializes the inventory back into the MY_ITEM block (the inverse of parseMyItemBlock).
         * Each pouch's items are written compacted from its offset (4 bytes each via the
         * InventoryItem7b layout), then the remaining slots up to maxSlots are zeroed. Because the
         * fromValue/toValue round-trip is byte-exact for valid items, a save that didn't touch items
         * reproduces the original block bytes. The block is re-hashed/re-encrypted by the caller.
         */
        for (auto& block : blocks) {
            if (block.key != MY_ITEM7_LGPE) continue;

            for (size_t p = 0; p < POUCH_COUNT7_LGPE; ++p) {
                const PouchInfo7LGPE& info = getPouchInfo7LGPE(static_cast<PouchType7LGPE>(p));
                const size_t itemCount = (p < items.size()) ? items[p].size() : 0;

                for (int i = 0; i < info.maxSlots; ++i) {
                    const size_t offset = static_cast<size_t>(info.offset) + static_cast<size_t>(i) * 4;
                    if (offset + 4 > block.data.size()) break;

                    uint32_t value = 0;  // empty slot
                    if (static_cast<size_t>(i) < itemCount) {
                        const InventoryItem& src = items[p][i];
                        InventoryItem7LGPE it;
                        it.itemId = src.itemId;
                        it.count = src.count;
                        it.isNew = src.isNew;
                        it.isFavorite = src.isFavorite;
                        value = it.toValue();
                    }
                    writeUInt32LittleEndian(&block.data[offset], value);
                }
            }

            logInfoToFile("Trainer7LGPE::updateItemBlock: item block serialized");
            break;
        }
    }

    // ========================================
    // Block Creation from Raw Save Data
    // ========================================

    std::vector<Save::Block> createBlocksFromSaveData7LGPE(const std::vector<uint8_t>& saveData)
    {
        /**
         * Converts raw Let's Go save data to Block structures.
         *
         * Let's Go uses fixed-offset blocks (not SCBlocks like Gen 8+).
         * This function creates Block structures from the raw data
         * for consistent handling with other generations.
         *
         * Block info verbatim from PKHeX BelugaBlockIndex.cs:
         * - idx 0  (MyItem)          @ 0x00000, len 0x00D90
         * - idx 2  (MyStatus)        @ 0x01000, len 0x00168
         * - idx 4  (Zukan)           @ 0x02A00, len 0x020E8
         * - idx 5  (Misc)            @ 0x04C00, len 0x00930
         * - idx 8  (PokeListHeader)  @ 0x05A00, len 0x00012  <- party header
         * - idx 9  (PokeListPokemon) @ 0x05C00, len 0x3F7A0
         * - idx 10 (PlayTime)        @ 0x45400, len 0x00008
         */

        std::vector<Save::Block> blocks;

        if (saveData.size() != SAVE_SIZE7_LGPE) {
            logErrorToFile("createBlocksFromSaveData7LGPE: Invalid save file size. Expected " +
                std::to_string(SAVE_SIZE7_LGPE) + " bytes, got " +
                std::to_string(saveData.size()) + " bytes.");
            return blocks;
        }

        // Define block info: {key, offset, size}
        struct BlockDef {
            size_t key;
            size_t offset;
            size_t size;
        };

        // Fixed-position blocks, offsets/lengths taken verbatim from PKHeX
        // BelugaBlockIndex. The party header (PokeListHeader) is idx 8 @ 0x05A00,
        // len 0x12 - previously guessed at 0x01200/0x04C00, which is what made
        // party/box loading unreliable and forced the old whole-save scan.
        const BlockDef blockDefs[] = {
            { MY_ITEM7_LGPE,           0x00000, 0x00D90 },  // idx 0  MyItem
            { MY_STATUS7_LGPE,         0x01000, 0x00168 },  // idx 2  MyStatus
            { ZUKAN7_LGPE,             0x02A00, 0x020E8 },  // idx 4  Zukan (pokedex)
            { MISC7_LGPE,              0x04C00, 0x00930 },  // idx 5  Misc (money)
            { POKE_LIST_HEADER7_LGPE,  0x05A00, 0x00012 },  // idx 8  party header
            { POKE_LIST_POKEMON7_LGPE, 0x05C00, 0x3F7A0 },  // idx 9  storage (1000 x 260 bytes)
            { PLAY_TIME7_LGPE,         0x45400, 0x00008 },  // idx 10 PlayTime
        };

        for (const auto& def : blockDefs) {
            if (def.offset + def.size <= saveData.size()) {
                Save::Block block;
                block.key = static_cast<uint32_t>(def.key);
                block.type = SCTypeCode::Array;
                block.data.assign(
                    saveData.begin() + def.offset,
                    saveData.begin() + def.offset + def.size
                );
                blocks.push_back(std::move(block));
            }
        }

        logInfoToFile("createBlocksFromSaveData7LGPE: Created " + std::to_string(blocks.size()) + " blocks from save data");

        return blocks;
    }

    // ========================================
    // Serialize edited blocks back into the raw save buffer (+ block checksums)
    // ========================================

    namespace {
        // CRC-16/ARC (reflected poly 0xA001, init 0x0000, no final XOR) — the LGPE block
        // checksum (PKHeX Checksums.CRC16NoInvert / BlockInfo7b). Computed over [p, p+n).
        uint16_t crc16Arc(const uint8_t* p, size_t n)
        {
            uint16_t crc = 0x0000;
            for (size_t i = 0; i < n; ++i) {
                crc ^= p[i];
                for (int b = 0; b < 8; ++b) {
                    crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                                    : static_cast<uint16_t>(crc >> 1);
                }
            }
            return crc;
        }

        // Active-area byte offset for a Beluga block key (matches createBlocksFromSaveData7LGPE).
        size_t offsetForBlockKey(uint32_t key)
        {
            switch (key) {
                case MY_ITEM7_LGPE:           return 0x00000;
                case MY_STATUS7_LGPE:         return 0x01000;
                case ZUKAN7_LGPE:             return 0x02A00;
                case MISC7_LGPE:              return 0x04C00;
                case POKE_LIST_HEADER7_LGPE:  return 0x05A00;
                case POKE_LIST_POKEMON7_LGPE: return 0x05C00;
                case PLAY_TIME7_LGPE:         return 0x45400;
                default:                      return SIZE_MAX;
            }
        }
    }

    void writeBlocksToSaveData7LGPE(std::vector<uint8_t>& raw, const std::vector<Save::Block>& blocks)
    {
        // In the "BEEF" footer, block id N's 2-byte CRC lives at 0xB861A + N*8.
        // (footer base 0xB8600 + 0x14 header + id*8 + 6 for the checksum field.)
        constexpr size_t CHECKSUM_BASE = 0xB861A;

        for (const auto& block : blocks) {
            const size_t offset = offsetForBlockKey(block.key);
            if (offset == SIZE_MAX) continue;

            const size_t len = block.data.size();
            if (offset + len > raw.size()) continue;

            // Patch the block bytes in place.
            std::memcpy(raw.data() + offset, block.data.data(), len);

            // Recompute this block's checksum into the footer. (Blocks we don't touch keep
            // their original bytes and valid checksums, so we only need to redo the ones here.)
            const uint16_t crc = crc16Arc(raw.data() + offset, len);
            const size_t chkOff = CHECKSUM_BASE + static_cast<size_t>(block.key) * 8;
            if (chkOff + 2 <= raw.size()) {
                writeUInt16LittleEndian(raw.data() + chkOff, crc);
            }
        }
    }
}
