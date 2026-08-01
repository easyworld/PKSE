/**
 * Trainer8LA.cpp - Generation 8 Trainer Implementation
 *
 * This file implements the Trainer8LA class for Pokemon Legends: Arceus save files.
 * Handles Gen 8-specific block parsing, Pokemon encryption/decryption, and
 * save file serialization.
 */
#include <algorithm>
#include <cstring>

#include "Trainer/Trainer8LA.h"
#include "Utils/Logger.h"

using namespace Utils;

namespace Trainer {
    // ========================================
    // Block Parsing Methods
    // ========================================

    void Trainer8LA::parseBlock(const Block& block)
    {
        switch (block.key) {
            case MY_STATUS8_LA:
                parseMyStatusBlock(block);
                break;
            case PARTY8_LA:
                parsePartyBlock(block);
                break;
            case MONEY8_LA:
                parseMoneyBlock(block);
                break;
            case ITEM_REGULAR8_LA:
            case ITEM_KEY8_LA:
            case ITEM_STORED8_LA:
            case ITEM_RECIPE8_LA:
                parseItemBlock(block);
                break;
            case BOX8_LA:
                parseBoxBlock(block);
                break;
            case BOX_LAYOUT8_LA:
                parseBoxLayoutBlock(block);
                break;
            case CURRENT_BOX8_LA:
                parseCurrentBoxBlock(block);
                break;
            case SAVE_REVISION8_LA:
                parseSaveRevisionBlock(block);
                break;
            // Additional blocks can be handled here
            default:
                // Unknown block - skip
                break;
        }
    }

    void Trainer8LA::parseMyStatusBlock(const Block& block)
    {
        /**
         * MyStatus8a Block Structure (Pokemon Legends: Arceus):
         * 0x10: ID32 (4 bytes) - TID16 (u16 @ 0x10) + SID16 (u16 @ 0x12)
         * 0x15: Trainer gender (1 byte)
         * 0x20: OT name (26 bytes / 13 UTF-16LE chars)
         *
         * ID32 format: SID16 << 16 | TID16
         * Display TID: ID32 % 1000000
         * Display SID: ID32 / 1000000
         */
        if (block.data.size() < 0x20 + 26) {
            logInfoToFile("Insufficient data in MY_STATUS block");
            return;
        }

        this->ID32 = readUInt32LittleEndian(&block.data[0x10]);
        this->TID16 = readUInt16LittleEndian(&block.data[0x10]);
        this->SID16 = readUInt16LittleEndian(&block.data[0x12]);
        this->TID = this->ID32 % 1000000;
        this->SID = this->ID32 / 1000000;
        size_t nameLength = 0x1A;  // 26 bytes = 13 UTF-16LE chars
        auto nameSpan = std::span<const uint8_t>(block.data.data() + 0x20, nameLength);
        this->trainerName = utf16ToUtf8(getString(nameSpan.data(), nameLength));
        this->trainerGender = block.data[0x15] & 1;   // 0x15: gender (0=M, 1=F)
        logInfoToFile("Parsed Trainer Name", this->trainerName.c_str());
    }

    void Trainer8LA::parsePartyBlock(const Block& block)
    {
        /**
         * PARTY Block Structure (Pokemon Legends: Arceus):
         * Pokemon stored back-to-back with no inter-slot gap:
         * - Slot 0: offset 0 (SIZE_PARTY8_LA bytes)
         * - Slot 1: offset SIZE_PARTY8_LA
         * - Slot 2: offset 2 * SIZE_PARTY8_LA
         * - ... up to 6 slots
         *
         * Each slot is exactly SIZE_PARTY8_LA (0x178) bytes of Pokemon data,
         * packed with no inter-slot gap.
         */
        const std::span<const std::byte> blockSpan(reinterpret_cast<const std::byte*>(block.data.data()), block.data.size());

        for (size_t slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
        {
            // Calculate offset to this slot (packed for LA)
            const size_t offset = slot * m_partySlotStride;
            if (offset + SIZE_PARTY8_LA > block.data.size())
                break;

            // Extract only the Pokemon data portion (SIZE_PARTY8_LA bytes)
            std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_PARTY8_LA);

            // Check if slot has valid Pokemon data (non-zero species)
            // The species ID is at offset 0x08 after decryption, but we can check
            // for an all-zero slot to skip empty slots
            bool isEmptySlot = true;
            for (size_t i = 0; i < SIZE_PARTY8_LA && i < slotSpan.size(); ++i) {
                if (slotSpan[i] != std::byte{0}) {
                    isEmptySlot = false;
                    break;
                }
            }

            if (!isEmptySlot) {
                // Decrypt and create Pokemon8LA object as unique_ptr
                // Pokemon8LA constructor handles decryption automatically
                party.push_back(std::make_unique<Pokemon8LA>(slotSpan));
            }
        }
    }

    void Trainer8LA::parseMoneyBlock(const Block& block)
    {
        /**
         * MONEY Block (Gen 8 Legends: Arceus):
         * For Gen 8 Legends: Arceus, the money value is stored directly as the block value.
         * 0x00: Money (4 bytes)
         */
        if (block.data.size() < 4) {
            return;
        }

        this->money = readUInt32LittleEndian(block.data.data());
    }

    void Trainer8LA::parseItemBlock(const Block& block)
    {
        // LA stores each pouch as a packed list of 4-byte entries { itemId u16 @0, count u16 @2 }.
        // Map this block's key to its pouch, then collect every non-empty (itemId != 0) entry.
        int pouch;
        switch (block.key) {
            case ITEM_REGULAR8_LA: pouch = static_cast<int>(PouchType8LA::Regular);  break;
            case ITEM_KEY8_LA:     pouch = static_cast<int>(PouchType8LA::KeyItems); break;
            case ITEM_STORED8_LA:  pouch = static_cast<int>(PouchType8LA::Stored);   break;
            case ITEM_RECIPE8_LA:  pouch = static_cast<int>(PouchType8LA::Recipes);  break;
            default: return;
        }

        if (items.size() < POUCH_COUNT8_LA) items.resize(POUCH_COUNT8_LA);
        items[pouch].clear();

        const size_t count = block.data.size() / ITEM_ENTRY_SIZE8_LA;
        for (size_t i = 0; i < count; ++i) {
            const size_t off = i * ITEM_ENTRY_SIZE8_LA;
            const uint16_t itemId = readUInt16LittleEndian(&block.data[off]);
            const uint16_t qty    = readUInt16LittleEndian(&block.data[off + 2]);
            if (itemId != 0) {
                items[pouch].push_back(InventoryItem{itemId, qty, false, false});
            }
        }
    }

    void Trainer8LA::parseBoxBlock(const Block& block)
    {
        /**
         * BOX Block Structure (Pokemon Legends: Arceus):
         * Pokemon stored back-to-back with no inter-slot gap:
         * - Box 0, Slot 0: offset 0 (SIZE_STORED8_LA bytes)
         * - Box 0, Slot 1: offset SIZE_STORED8_LA
         * - ... Box 0, Slot 29: offset 29 * SIZE_STORED8_LA
         * - Box 1, Slot 0: offset 30 * SIZE_STORED8_LA
         * - ... etc for all 32 boxes
         *
         * Each slot is exactly SIZE_STORED8_LA (0x168) bytes of Pokemon data,
         * packed with no inter-slot gap.
         *
         * Total size: 32 boxes * 30 slots * SIZE_STORED8_LA bytes
         */
        const std::span<const std::byte> blockSpan(
            reinterpret_cast<const std::byte*>(block.data.data()),
            block.data.size()
        );

        for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_LA; ++boxIndex) {
            for (size_t slot = 0; slot < BOX_SLOTS; ++slot) {
                // Calculate offset (packed for LA)
                const size_t offset = (boxIndex * BOX_SLOTS + slot) * m_boxSlotStride;
                if (offset + SIZE_STORED8_LA > block.data.size()) {
                    break;
                }

                // Extract only the Pokemon data portion (SIZE_STORED8_LA bytes)
                std::span<const std::byte> slotSpan = blockSpan.subspan(offset, SIZE_STORED8_LA);

                // Check if slot has a Pokemon (non-zero data)
                bool isEmptySlot = true;
                for (size_t i = 0; i < SIZE_STORED8_LA && i < slotSpan.size(); ++i) {
                    if (slotSpan[i] != std::byte{0}) {
                        isEmptySlot = false;
                        break;
                    }
                }

                if (!isEmptySlot) {
                    // Decrypt and create Pokemon8LA object
                    boxes[boxIndex][slot] = std::make_unique<Pokemon8LA>(slotSpan);
                } else {
                    // Empty slot
                    boxes[boxIndex][slot] = nullptr;
                }
            }
        }
    }

    void Trainer8LA::parseBoxLayoutBlock(const Block& block)
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
        for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_LA; ++boxIndex) {
            size_t offset = boxIndex * BOX_NAME_LENGTH8_LA;
            if (offset + BOX_NAME_LENGTH8_LA <= block.data.size()) {
                // Extract box name (UTF-16LE string)
                std::u16string boxNameU16 = getString(
                    block.data.data() + offset,
                    BOX_NAME_LENGTH8_LA
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

    void Trainer8LA::parseSaveRevisionBlock(const Block&)
    { /* LA save-revision detection deferred */ }

    // ========================================
    // Block Update Methods
    // ========================================

    void Trainer8LA::updatePartyBlock()
    {
        /**
         * Updates the PARTY block with modified Pokemon data (Legends: Arceus / PA8).
         *
         * PARTY slots are PARTY-size (SIZE_PARTY8_LA = 0x178): the full 0x168 stored
         * region plus the 0x10-byte party-stats tail. Each occupied slot gets the full
         * 0x178 encrypted bytes.
         *
         * Empty party slots must be the game's encrypted "blank" (which DECRYPTS to species 0),
         * NOT literal zeros — exactly like updateBoxBlock(). The game decrypts every slot and
         * validates it, so a zeroed slot decrypts to garbage and renders a BAD EGG. This path
         * used to memset(0) and corrupted the party into Bad Eggs on any save (even a box-name
         * edit, since the party is re-serialized unconditionally). The PartyCount byte at
         * 6*0x178 is deliberately left untouched: empty slots load as species-0 "ghosts" that
         * inflate party.size(), so the save's own count is authoritative, not party.size().
         */
        for (auto& block : blocks) {
            if (block.key == PARTY8_LA) {
                // Ensure the block data is large enough (including gaps)
                size_t requiredSize = MAX_PARTY_SLOTS * m_partySlotStride;
                if (block.data.size() < requiredSize) {
                    block.data.resize(requiredSize, 0);
                }

                // Synthesize a party-size (0x178) encrypted blank that decrypts to species 0:
                // an all-zero PA8 encrypted with EC 0 (== PKHeX's new PA8() BlankPKM).
                std::vector<uint8_t> blankSlot;
                {
                    std::vector<std::byte> zero(SIZE_PARTY8_LA, std::byte{0});
                    std::byte* enc = encryptArray8LA(
                        std::span<const std::byte>(zero.data(), SIZE_PARTY8_LA), 0);
                    blankSlot.assign(reinterpret_cast<const uint8_t*>(enc),
                                     reinterpret_cast<const uint8_t*>(enc) + SIZE_PARTY8_LA);
                    blankSlot.resize(m_partySlotStride, 0);  // 0x178 (no gap for LA)
                    delete[] enc;
                }

                // Write each party Pokemon
                for (size_t i = 0; i < party.size() && i < MAX_PARTY_SLOTS; ++i) {
                    // Calculate offset (packed for LA: stride == SIZE_PARTY8_LA)
                    const size_t offset = i * m_partySlotStride;

                    if (party[i] && party[i]->speciesID() != 0) {
                        // Pokemon exists - encrypt and write
                        const ::Pokemon::Pokemon* pokemon = party[i].get();
                        uint32_t ec = readUInt32LittleEndian(
                            reinterpret_cast<const uint8_t*>(pokemon->getData().data())
                        );

                        // Create span of decrypted Pokemon data (full party size, 0x178)
                        std::span<const std::byte> decryptedSpan(
                            pokemon->getData().data(),
                            pokemon->getDataSize()
                        );

                        // Encrypt the Pokemon data (PA8 crypto)
                        std::byte* encryptedData = encryptArray8LA(decryptedSpan, ec);

                        // Write the FULL party-size (0x178) encrypted bytes into the slot
                        std::memcpy(&block.data[offset], encryptedData, pokemon->getDataSize());

                        // Zero out the gap after the Pokemon data (0 bytes for LA's packed layout)
                        std::memset(&block.data[offset + SIZE_PARTY8_LA], 0, m_slotGapZero);

                        // Clean up encrypted buffer
                        delete[] encryptedData;
                    } else {
                        // Empty slot: write the encrypted blank, NOT zeros (see the header note).
                        std::memcpy(&block.data[offset], blankSlot.data(), m_partySlotStride);
                    }
                }

                // Remaining trailing slots are empty -> encrypted blank, not zeros.
                for (size_t i = party.size(); i < MAX_PARTY_SLOTS; ++i) {
                    const size_t offset = i * m_partySlotStride;
                    std::memcpy(&block.data[offset], blankSlot.data(), m_partySlotStride);
                }

                break;
            }
        }
    }

    void Trainer8LA::parseCurrentBoxBlock(const Block& block)
    {
        // "U8 Box Index" -- a scalar block PKHeX reads as a byte (0..31 fits one byte).
        if (block.data.empty()) return;
        uint8_t box = block.data[0];
        if (box < BOX_COUNT8_LA) this->currentBox = box;
    }

    void Trainer8LA::updateCurrentBoxBlock()
    {
        // Inverse of parseCurrentBoxBlock: write the low byte and clear the rest of the scalar.
        for (auto& block : blocks) {
            if (block.key != CURRENT_BOX8_LA || block.data.empty()) continue;
            block.data[0] = static_cast<uint8_t>(currentBox);
            for (size_t i = 1; i < block.data.size() && i < 4; ++i) block.data[i] = 0;
            break;
        }
    }

    void Trainer8LA::updateBoxNameBlock()
    {
        // Inverse of parseBoxLayoutBlock. See Trainer8SWSH::updateBoxNameBlock for why the block is
        // bounds-checked rather than resized.
        for (auto& block : blocks) {
            if (block.key != BOX_LAYOUT8_LA) continue;
            for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_LA && boxIndex < boxNames.size(); ++boxIndex) {
                if (!isBoxNameDirty(boxIndex)) continue;   // never persist a display default
                const size_t offset = boxIndex * BOX_NAME_LENGTH8_LA;
                if (offset + BOX_NAME_LENGTH8_LA > block.data.size()) break;
                setString(block.data.data() + offset, BOX_NAME_LENGTH8_LA,
                          utf8ToUtf16(boxNames[boxIndex]), BOX_NAME_LENGTH8_LA / 2 - 1);
            }
            break;
        }
    }

    void Trainer8LA::updateBoxBlock()
    {
        /**
         * Updates the BOX block with modified Pokemon data (Legends: Arceus / PA8).
         *
         * CRITICAL DIVERGENCE from the party block: BOX slots are STORED-size
         * (SIZE_STORED8_LA = 0x168) — they have NO party-stats region. For each occupied
         * slot we still encrypt the mon's FULL party-size buffer (0x178) via encryptArray8LA
         * (crypto shuffles/XORs the 0x08..0x168 blocks; the 0x10 party tail is irrelevant
         * to a box slot), then copy ONLY the first 0x168 encrypted bytes into the slot.
         *
         * Empty box slots must be the game's encrypted "blank" (which DECRYPTS to species 0),
         * NOT literal zeros: the game decrypts every box slot and checks species, so a zeroed
         * slot decrypts to garbage and renders a BAD EGG. Reuse the game's own blank by copying
         * an existing empty (species-0) slot; if every box is full, synthesize one from an
         * all-zero PA8 encrypted with EC 0 and take its first 0x168 bytes.
         */
        for (auto& block : blocks) {
            if (block.key == BOX8_LA) {
                // Ensure the block data is large enough for all boxes (STORED-size slots)
                size_t requiredSize = BOX_COUNT8_LA * BOX_SLOTS * m_boxSlotStride;
                if (block.data.size() < requiredSize) {
                    block.data.resize(requiredSize, 0);
                }

                // Capture a reference "blank" slot (0x168 bytes) that decrypts to species 0.
                std::vector<uint8_t> blankSlot;
                for (size_t bi = 0; bi < BOX_COUNT8_LA && blankSlot.empty(); ++bi) {
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
                    // Synthesize: encrypt an all-zero PA8 (party size) with EC 0, then take the
                    // first SIZE_STORED8_LA (0x168) bytes — a stored-size blank that decrypts to species 0.
                    std::vector<std::byte> zero(SIZE_PARTY8_LA, std::byte{0});
                    std::byte* enc = encryptArray8LA(
                        std::span<const std::byte>(zero.data(), SIZE_PARTY8_LA), 0);
                    blankSlot.assign(reinterpret_cast<const uint8_t*>(enc),
                                     reinterpret_cast<const uint8_t*>(enc) + SIZE_STORED8_LA);
                    blankSlot.resize(m_boxSlotStride, 0);  // exactly 0x168 for LA's packed layout
                    delete[] enc;
                }

                // Write each Pokemon back to the block
                for (size_t boxIndex = 0; boxIndex < BOX_COUNT8_LA; ++boxIndex) {
                    for (size_t slot = 0; slot < BOX_SLOTS; ++slot) {
                        // Calculate offset (packed for LA: stride == SIZE_STORED8_LA = 0x168)
                        const size_t offset = (boxIndex * BOX_SLOTS + slot) * m_boxSlotStride;

                        // Gate on species, not just the pointer (ghost-slot fix). A slot left holding a
                        // non-null but blank/species-0 mon (e.g. after a bank move) must read as EMPTY
                        // in-game — re-encrypting a blank writes a bad egg.
                        if (boxes[boxIndex][slot] && boxes[boxIndex][slot]->speciesID() != 0) {
                            // Pokemon exists - encrypt and write
                            const auto& pokemon = boxes[boxIndex][slot];

                            // Get the Encryption Constant (used as seed for encryption)
                            uint32_t ec = readUInt32LittleEndian(
                                reinterpret_cast<const uint8_t*>(pokemon->getData().data())
                            );

                            // Create span of decrypted Pokemon data (full party size, 0x178)
                            std::span<const std::byte> decryptedSpan(
                                pokemon->getData().data(),
                                pokemon->getDataSize()
                            );

                            // Encrypt the FULL 0x178 buffer (PA8 crypto)
                            std::byte* encryptedData = encryptArray8LA(decryptedSpan, ec);

                            // Box slots are STORED-size: copy ONLY the first 0x168 encrypted bytes.
                            // (No party-stats region and no inter-slot gap for a box slot.)
                            std::memcpy(&block.data[offset], encryptedData, SIZE_STORED8_LA);

                            // Clean up encrypted buffer
                            delete[] encryptedData;
                        } else {
                            // Empty/cleared slot: write the game's encrypted blank (0x168), NOT zeros.
                            // Zeros decrypt to garbage in-game and show as a BAD EGG in every empty slot.
                            std::memcpy(&block.data[offset], blankSlot.data(), m_boxSlotStride);
                        }
                    }
                }
                break;
            }
        }
    }

    std::unique_ptr<::Pokemon::Pokemon> Trainer8LA::createBlankPokemon() const
    {
        // Mirror updateBoxBlock()'s encrypted-blank synth: a zeroed *decrypted* party-size PA8
        // buffer (0x178) encrypted with EC/seed 0. LA entities keep a party-size buffer (box slots
        // are stored-size 0x168, party slots 0x178), so construct from the FULL party-size encrypted
        // span (matches the party read). The ctor decrypts it straight back to zeros -> a clean
        // species-0, checksum-valid entity. Raw zeros would decrypt to garbage (BAD EGG).
        std::vector<std::byte> zero(SIZE_PARTY8_LA, std::byte{0});
        std::byte* enc = encryptArray8LA(
            std::span<const std::byte>(zero.data(), SIZE_PARTY8_LA), 0);
        auto p = std::make_unique<Pokemon8LA>(
            std::span<const std::byte>(enc, SIZE_PARTY8_LA));
        delete[] enc;
        return p;
    }

    void Trainer8LA::updateItemBlock()
    {
        // Rewrite each pouch block in place from PKSE's parsed list. Safe to rebuild: we parse EVERY
        // non-zero entry (no unknown items to lose, unlike the indexed Gen 9 pouches). Zero the block,
        // then write the entries compacted to the front — trailing zeros are empty slots. The block
        // size is preserved (the game reads the full capacity and ignores itemId==0 entries).
        struct PouchBlock { size_t key; PouchType8LA pouch; };
        const PouchBlock pouches[] = {
            { ITEM_REGULAR8_LA, PouchType8LA::Regular  },
            { ITEM_KEY8_LA,     PouchType8LA::KeyItems },
            { ITEM_STORED8_LA,  PouchType8LA::Stored   },
            { ITEM_RECIPE8_LA,  PouchType8LA::Recipes  },
        };
        for (const auto& pb : pouches) {
            const size_t idx = static_cast<size_t>(pb.pouch);
            if (idx >= items.size()) continue;
            for (auto& block : blocks) {
                if (block.key != pb.key) continue;
                std::memset(block.data.data(), 0, block.data.size());
                const size_t capacity = block.data.size() / ITEM_ENTRY_SIZE8_LA;
                size_t slot = 0;
                for (const auto& item : items[idx]) {
                    if (slot >= capacity) break;
                    const size_t off = slot * ITEM_ENTRY_SIZE8_LA;
                    block.data[off]     = static_cast<uint8_t>(item.itemId & 0xFF);
                    block.data[off + 1] = static_cast<uint8_t>((item.itemId >> 8) & 0xFF);
                    block.data[off + 2] = static_cast<uint8_t>(item.count & 0xFF);
                    block.data[off + 3] = static_cast<uint8_t>((item.count >> 8) & 0xFF);
                    ++slot;
                }
                break;
            }
        }
    }

    // Per-pouch capacity, matching PKHeX PlayerBag8a. The three specialty pouches are fixed; the
    // general Items bag grows with the player's Satchel Upgrades (0-39): min(675, upgrades + 20).
    // updateItemBlock already clamps writes to the block's own size, so this only gates the UI's
    // add-item flow -- it can't overflow anything.
    size_t Trainer8LA::getItemPouchCapacity(int pouch) const {
        switch (static_cast<PouchType8LA>(pouch)) {
            case PouchType8LA::KeyItems: return POUCH_CAP_KEY8_LA;      // 100
            case PouchType8LA::Stored:   return POUCH_CAP_STORED8_LA;   // 180
            case PouchType8LA::Recipes:  return POUCH_CAP_RECIPE8_LA;   // 70
            case PouchType8LA::Regular: {
                uint32_t upgrades = 0;
                for (const auto& b : blocks) {
                    if (b.key == SATCHEL_UPGRADES8_LA) {
                        if (b.data.size() >= 4) upgrades = readUInt32LittleEndian(b.data.data());
                        break;
                    }
                }
                const uint32_t cap = upgrades + 20;
                return cap < POUCH_CAP_REGULAR_MAX8_LA ? cap : POUCH_CAP_REGULAR_MAX8_LA;
            }
        }
        return 0;
    }
}
