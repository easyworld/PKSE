/**
 * Trainer8BDSP.cpp - Brilliant Diamond / Shining Pearl (BDSP) trainer implementation.
 *
 * Unlike the SwishCrypto games, BDSP is a FLAT fixed-offset save (SAV8BDSP): a plaintext
 * little-endian blob guarded by one whole-file MD5. Entities (PB8) are the same 0x158
 * Gen-8 format as Sword/Shield, held encrypted at fixed offsets. Trainer8BDSP keeps the raw
 * save buffer and reads/writes fixed offsets rather than parsing keyed blocks.
 *
 * Write path: updatePartyBlock/updateBoxBlock re-encrypt entities in place into saveData; on save,
 * recomputeHash() rewrites the whole-file MD5. Empty box slots get the game's encrypted blank.
 */
#include <algorithm>
#include <cstring>

#include "Trainer/Trainer8BDSP.h"
#include "Utils/MD5.h"
#include "Utils/Logger.h"

using namespace Utils;

namespace Trainer {

    Trainer8BDSP::Trainer8BDSP(std::vector<uint8_t> data)
        : Trainer(std::vector<Block>{}), saveData(std::move(data))
    {
        parseMyStatus();
        parseParty();
        parseBoxes();
        parseBoxNames();
        parseItems();
    }

    void Trainer8BDSP::parseMyStatus()
    {
        if (saveData.size() < BDSP_ROMCODE + 1) {
            logErrorToFile("BDSP save too small for MyStatus block");
            return;
        }
        this->ID32  = readUInt32LittleEndian(&saveData[BDSP_ID32]);
        this->TID16 = readUInt16LittleEndian(&saveData[BDSP_ID32]);
        this->SID16 = readUInt16LittleEndian(&saveData[BDSP_ID32 + 2]);
        this->TID   = this->ID32 % 1000000;
        this->SID   = this->ID32 / 1000000;
        this->money = readUInt32LittleEndian(&saveData[BDSP_MONEY]);

        // OT name: UTF-16LE, 0x1A bytes (13 chars) at BDSP_MYSTATUS.
        this->trainerName = utf16ToUtf8(getString(&saveData[BDSP_MYSTATUS], 0x1A));
        this->trainerGender = (saveData[BDSP_MYSTATUS + 0x24] == 1) ? 0 : 1;   // 0x24: Male flag (1=M -> gender 0)
        logInfoToFile("Parsed BDSP Trainer Name", this->trainerName.c_str());
    }

    void Trainer8BDSP::parseParty()
    {
        party.clear();
        uint8_t count = saveData[BDSP_PARTY_COUNT];
        if (count > MAX_PARTY_SLOTS) count = MAX_PARTY_SLOTS;
        for (uint8_t i = 0; i < count; ++i) {
            const size_t off = BDSP_PARTY_OFFSET + static_cast<size_t>(i) * SIZE_PARTY8_BDSP;
            if (off + SIZE_PARTY8_BDSP > saveData.size()) break;
            std::span<const std::byte> slot(
                reinterpret_cast<const std::byte*>(&saveData[off]), SIZE_PARTY8_BDSP);
            auto mon = std::make_unique<Pokemon8BDSP>(slot);
            if (mon->speciesID() != 0) party.push_back(std::move(mon));
        }
    }

    void Trainer8BDSP::parseBoxes()
    {
        boxes.clear();
        boxes.resize(BDSP_BOX_COUNT);
        for (size_t b = 0; b < BDSP_BOX_COUNT; ++b) {
            for (size_t s = 0; s < BOX_SLOTS; ++s) {
                const size_t off = BDSP_BOX_OFFSET + (b * BOX_SLOTS + s) * SIZE_PARTY8_BDSP;
                if (off + SIZE_PARTY8_BDSP > saveData.size()) return;
                std::span<const std::byte> slot(
                    reinterpret_cast<const std::byte*>(&saveData[off]), SIZE_PARTY8_BDSP);
                auto mon = std::make_unique<Pokemon8BDSP>(slot);
                if (mon->speciesID() != 0) {
                    boxes[b][s] = std::move(mon);
                } else if (m_boxBlank.empty()) {
                    // Capture the game's encrypted blank (an empty slot) once, for empty-slot writes.
                    m_boxBlank.assign(&saveData[off], &saveData[off] + SIZE_PARTY8_BDSP);
                }
                // occupied stored above; empty slots stay nullptr
            }
        }
    }

    void Trainer8BDSP::updateBoxNameBlock()
    {
        // Inverse of parseBoxNames. BDSP is a flat MD5-guarded blob rather than SCBlocks, so this
        // writes straight into saveData -- and must run BEFORE recomputeHash(), since the whole-file
        // MD5 covers this region.
        for (size_t b = 0; b < BDSP_BOX_COUNT && b < boxNames.size(); ++b) {
            if (!isBoxNameDirty(b)) continue;   // never persist a display default (#50)
            const size_t off = BDSP_BOXNAME_OFFSET + b * BDSP_BOXNAME_LENGTH;
            if (off + BDSP_BOXNAME_LENGTH > saveData.size()) break;
            setString(&saveData[off], BDSP_BOXNAME_LENGTH, utf8ToUtf16(boxNames[b]),
                      BDSP_BOXNAME_LENGTH / 2 - 1);
        }
    }

    void Trainer8BDSP::updateCurrentBoxBlock()
    {
        // Single u8 in the flat save (BoxLayout8b.CurrentBox). Like updateBoxNameBlock this writes
        // straight into saveData and must run BEFORE recomputeHash() (the MD5 covers this byte).
        if (BDSP_CURRENTBOX < saveData.size())
            saveData[BDSP_CURRENTBOX] = static_cast<uint8_t>(currentBox);
    }

    void Trainer8BDSP::parseBoxNames()
    {
        boxNames.clear();
        for (size_t b = 0; b < BDSP_BOX_COUNT; ++b) {
            const size_t off = BDSP_BOXNAME_OFFSET + b * 0x22;
            std::string name;
            if (off + 0x22 <= saveData.size())
                name = utf16ToUtf8(getString(&saveData[off], 0x22));
            if (name.empty())
                name = "盒子 " + std::to_string(b + 1);
            boxNames.push_back(name);
        }
        // Current box (single u8 in the same BoxLayout region), clamped defensively.
        if (BDSP_CURRENTBOX < saveData.size() && saveData[BDSP_CURRENTBOX] < BDSP_BOX_COUNT)
            currentBox = saveData[BDSP_CURRENTBOX];
    }

    void Trainer8BDSP::parseItems()
    {
        // BDSP items are indexed by item id: item i's 0x10-byte record is at BDSP_ITEM_BASE + i*0x10,
        // Count is int32 at record offset 0. Collect owned (count > 0) items into each pouch.
        items.clear();
        items.resize(POUCH_COUNT8BDSP);
        for (size_t p = 0; p < POUCH_COUNT8BDSP; ++p) {
            const auto& validIds = getValidItemIds8BDSP(static_cast<PouchType8BDSP>(p));
            for (uint16_t id : validIds) {
                const size_t off = BDSP_ITEM_BASE + static_cast<size_t>(id) * ITEM_ENTRY_SIZE8BDSP;
                if (off + 4 > saveData.size()) continue;
                const uint32_t count = readUInt32LittleEndian(&saveData[off + ITEM_COUNT_OFFSET8BDSP]);
                if (count > 0) {
                    items[p].push_back(InventoryItem{
                        id, static_cast<uint16_t>(std::min<uint32_t>(count, 0xFFFFu)), false, false});
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Write path
    // ------------------------------------------------------------------
    void Trainer8BDSP::updatePartyBlock()
    {
        // Write each party slot in place (encrypted PB8), then set the party count byte. Empty slots
        // are zeroed — the count gates how many the game reads.
        for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) {
            const size_t off = BDSP_PARTY_OFFSET + i * SIZE_PARTY8_BDSP;
            if (off + SIZE_PARTY8_BDSP > saveData.size()) break;
            if (i < party.size() && party[i] && party[i]->speciesID() != 0) {
                const auto& pk = party[i];
                const uint32_t ec = readUInt32LittleEndian(
                    reinterpret_cast<const uint8_t*>(pk->getData().data()));
                std::span<const std::byte> dec(pk->getData().data(), pk->getDataSize());
                std::byte* enc = encryptArray8BDSP(dec, ec);
                std::memcpy(&saveData[off], enc, SIZE_PARTY8_BDSP);
                delete[] enc;
            } else {
                std::memset(&saveData[off], 0, SIZE_PARTY8_BDSP);
            }
        }
        if (BDSP_PARTY_COUNT < saveData.size())
            saveData[BDSP_PARTY_COUNT] =
                static_cast<uint8_t>(std::min<size_t>(party.size(), MAX_PARTY_SLOTS));
    }

    void Trainer8BDSP::updateBoxBlock()
    {
        // Rewrite the whole box region: occupied slots as encrypted PB8; empty slots as the game's
        // own encrypted blank (NOT zeros — the game decrypts every slot and checks species, so zeros
        // would render as a Bad Egg; same lesson as the SwishCrypto games).
        std::vector<uint8_t> blank = m_boxBlank;
        if (blank.empty()) {
            std::vector<std::byte> zero(SIZE_PARTY8_BDSP, std::byte{0});
            std::byte* enc = encryptArray8BDSP(
                std::span<const std::byte>(zero.data(), SIZE_PARTY8_BDSP), 0);
            blank.assign(reinterpret_cast<const uint8_t*>(enc),
                         reinterpret_cast<const uint8_t*>(enc) + SIZE_PARTY8_BDSP);
            delete[] enc;
        }
        for (size_t b = 0; b < BDSP_BOX_COUNT; ++b) {
            for (size_t s = 0; s < BOX_SLOTS; ++s) {
                const size_t off = BDSP_BOX_OFFSET + (b * BOX_SLOTS + s) * SIZE_PARTY8_BDSP;
                if (off + SIZE_PARTY8_BDSP > saveData.size()) return;
                if (boxes[b][s] && boxes[b][s]->speciesID() != 0) {
                    const auto& pk = boxes[b][s];
                    const uint32_t ec = readUInt32LittleEndian(
                        reinterpret_cast<const uint8_t*>(pk->getData().data()));
                    std::span<const std::byte> dec(pk->getData().data(), pk->getDataSize());
                    std::byte* enc = encryptArray8BDSP(dec, ec);
                    std::memcpy(&saveData[off], enc, SIZE_PARTY8_BDSP);
                    delete[] enc;
                } else {
                    std::memcpy(&saveData[off], blank.data(), SIZE_PARTY8_BDSP);
                }
            }
        }
    }

    std::unique_ptr<::Pokemon::Pokemon> Trainer8BDSP::createBlankPokemon() const
    {
        // Mirror updateBoxBlock()'s encrypted-blank fallback: a zeroed *decrypted* PB8 buffer
        // encrypted with EC/seed 0, then fed to the ctor (which decrypts it straight back to zeros)
        // -> a clean species-0, checksum-valid entity. Raw zeros in the ctor would decrypt to garbage
        // (BAD EGG); the encrypt->decrypt round-trip is what makes the blank valid.
        std::vector<std::byte> zero(SIZE_PARTY8_BDSP, std::byte{0});
        std::byte* enc = encryptArray8BDSP(
            std::span<const std::byte>(zero.data(), SIZE_PARTY8_BDSP), 0);
        auto p = std::make_unique<Pokemon8BDSP>(
            std::span<const std::byte>(enc, SIZE_PARTY8_BDSP));
        delete[] enc;
        return p;
    }

    void Trainer8BDSP::updateItemBlock()
    {
        // Write each parsed item's count (int32) in place — non-destructive: touches only known items,
        // leaving unrelated item records and the new/favorite/sort-order fields intact.
        for (size_t p = 0; p < items.size() && p < POUCH_COUNT8BDSP; ++p) {
            for (const auto& item : items[p]) {
                const size_t off = BDSP_ITEM_BASE + static_cast<size_t>(item.itemId) * ITEM_ENTRY_SIZE8BDSP;
                if (off + 4 > saveData.size()) continue;
                saveData[off + 0] = static_cast<uint8_t>(item.count & 0xFF);
                saveData[off + 1] = static_cast<uint8_t>((item.count >> 8) & 0xFF);
                saveData[off + 2] = 0;
                saveData[off + 3] = 0;
                // IsNew is an int32 at record 0x4. Only SET it for freshly-added items (#E6) so the bag
                // shows the "new" marker; existing new/favorite/sort fields stay intact. (BDSP derives
                // the pouch from the item id, so unlike S/V-Z/A it needs no pouchId stamp.)
                if (item.isNew && item.count > 0 && off + 8 <= saveData.size()) {
                    saveData[off + 4] = 1;
                    saveData[off + 5] = 0;
                    saveData[off + 6] = 0;
                    saveData[off + 7] = 0;
                }
            }
        }
    }

    void Trainer8BDSP::recomputeHash()
    {
        // Zero the 16 hash bytes, MD5 the ENTIRE buffer, write the digest back in place.
        // BDSP silently rejects a save whose hash doesn't match. Ready for the write path.
        if (saveData.size() < BDSP_HASH_OFFSET + 16) return;
        std::memset(&saveData[BDSP_HASH_OFFSET], 0, 16);
        Utils::md5(saveData.data(), saveData.size(), &saveData[BDSP_HASH_OFFSET]);
    }
}
