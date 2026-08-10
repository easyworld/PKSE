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
            if (!isBoxNameDirty(b)) continue;   // never persist a display default
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
        // Write each party slot in place (encrypted PB8), then set the party count byte.
        //
        // Empty slots get the game's own encrypted blank, NOT zeros — the same rule updateBoxBlock
        // follows, and for the same reason: the game decrypts a slot before it looks at it, so
        // zeros decrypt to garbage and render a Bad Egg. The count is *supposed* to gate how many
        // slots are read, but leaning on that is how the SwishCrypto games ended up writing Bad
        // Eggs into every empty party slot. A blank is correct whether or not the count is.
        std::vector<uint8_t> blank = m_boxBlank;
        if (blank.empty()) {
            std::vector<std::byte> zero(SIZE_PARTY8_BDSP, std::byte{0});
            std::byte* enc = encryptArray8BDSP(
                std::span<const std::byte>(zero.data(), SIZE_PARTY8_BDSP), 0);
            blank.assign(reinterpret_cast<const uint8_t*>(enc),
                         reinterpret_cast<const uint8_t*>(enc) + SIZE_PARTY8_BDSP);
            delete[] enc;
        }
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
                // Already reads as empty? Leave the game's own bytes exactly as they are. Empty
                // slots carry stale per-slot bytes the game never cleared, so stamping one
                // canonical blank over them would rewrite a party nobody edited.
                //
                // The all-zero test is not redundant with the species test: the cipher seeds its
                // PRNG with the encryption constant, so a zeroed slot (EC 0, first PRNG word
                // 0x0000) ALSO decrypts to species 0 while the rest is checksum-rejected garbage.
                // Without this, a slot an older build zeroed would be mistaken for a valid blank
                // and never repaired.
                bool anyNonZero = false;
                for (size_t k = 0; k < SIZE_PARTY8_BDSP && !anyNonZero; ++k)
                    anyNonZero = (saveData[off + k] != 0);
                if (anyNonZero) {
                    std::span<const std::byte> slot(
                        reinterpret_cast<const std::byte*>(&saveData[off]), SIZE_PARTY8_BDSP);
                    Pokemon8BDSP existing(slot);
                    if (existing.speciesID() == 0) continue;
                }
                std::memcpy(&saveData[off], blank.data(), SIZE_PARTY8_BDSP);
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

    // ---- Pokedex (ZUKAN_WORK @ 0x7A328, size 0x30B8) -----------------------------------------
    //
    // BDSP stores none of this as bitfields. Everything is a 4-byte-aligned array indexed by
    // (species - 1), covering the Sinnoh-era National Dex 1..493 (PKHeX Zukan8b):
    //
    //   0x0000  u32[493] state        0 None / 1 HeardOf / 2 Seen / 3 Caught  -- a VALUE, not a flag
    //   0x07B4  u32[493] male shiny   \  "have you seen this gender/shininess", one u32 per species
    //   0x0F68  u32[493] female shiny  |  each holding 0 or 1
    //   0x171C  u32[493] male          |
    //   0x1ED0  u32[493] female       /
    //   0x2684  per-species FORM arrays (see FORM_ARRAYS), each immediately followed by its shiny twin
    //   0x28FC  u32[493] language flags
    //   0x30B0  regional dex obtained    0x30B4  national dex obtained
    //
    // The offsets chain from the array sizes and land exactly on 0x30B0 / 0x30B4 / 0x30B8, which is
    // what makes them checkable rather than copied.
    namespace {
        constexpr size_t BDSP_DEX          = 0x7A328;   // ZUKAN_WORK, absolute in saveData
        constexpr size_t BDSP_DEX_SIZE     = 0x30B8;
        constexpr uint16_t BDSP_MAX_SPECIES = 493;      // Arceus
        constexpr size_t OFS_STATE         = 0x0000;
        constexpr size_t OFS_MALE_SHINY    = 0x07B4;
        constexpr size_t OFS_FEMALE_SHINY  = 0x0F68;
        constexpr size_t OFS_MALE          = 0x171C;
        constexpr size_t OFS_FEMALE        = 0x1ED0;
        constexpr size_t OFS_LANGUAGE      = 0x28FC;
        constexpr uint32_t ZUKAN_CAUGHT    = 3;         // ZukanState8b.Caught

        // Species that have a per-form array, its entry count, and where the NON-shiny array starts.
        // The shiny twin follows immediately, count * 4 bytes later.
        struct BdspFormArray { uint16_t species; uint16_t count; size_t offset; };
        constexpr BdspFormArray FORM_ARRAYS[] = {
            { 201, 28,  9860 },   // Unown
            { 351,  4, 10084 },   // Castform
            { 386,  4, 10116 },   // Deoxys
            { 412,  3, 10148 },   // Burmy
            { 413,  3, 10172 },   // Wormadam
            { 414,  3, 10196 },   // Mothim
            { 421,  2, 10220 },   // Cherrim
            { 422,  2, 10236 },   // Shellos
            { 423,  2, 10252 },   // Gastrodon
            { 479,  6, 10268 },   // Rotom
            { 487,  2, 10316 },   // Giratina
            { 492,  2, 10332 },   // Shaymin
            { 493, 18, 10348 },   // Arceus
        };

        // Language id -> bit. Slot 6 is unused, so ids 7+ shift down by two (PKHeX GetLanguageBit).
        int bdspLangBit(uint8_t language) {
            if (language == 0 || language == 6 || language > 10) return -1;
            return (language >= 7) ? language - 2 : language - 1;
        }
    }

    void Trainer8BDSP::updatePokedexBlock()
    {
        if (saveData.size() < BDSP_DEX + BDSP_DEX_SIZE) return;   // not a layout we recognise

        auto putU32 = [&](size_t rel, uint32_t v) {
            writeUInt32LittleEndian(&saveData[BDSP_DEX + rel], v);
        };
        auto getU32 = [&](size_t rel) {
            return readUInt32LittleEndian(&saveData[BDSP_DEX + rel]);
        };

        auto registerMon = [&](const ::Pokemon::Pokemon* pk) {
            if (!pk || pk->isEgg()) return;
            const uint16_t species = pk->speciesID();
            if (species == 0 || species > BDSP_MAX_SPECIES) return;   // BDSP's dex stops at Arceus

            const size_t i = static_cast<size_t>(species - 1) * 4;
            const bool shiny = pk->isShiny(pk->id32(), pk->species());

            // State is a value, not a flag: only ever raise it, so a Pokemon already Caught is not
            // knocked back down and a Seen one is promoted.
            if (getU32(OFS_STATE + i) < ZUKAN_CAUGHT) putU32(OFS_STATE + i, ZUKAN_CAUGHT);

            // Gender/shiny "have seen" markers. A GENDERLESS Pokemon sets BOTH, which is what the
            // games do -- it is not a male-by-default case like the other formats.
            const uint8_t gender = pk->gender();
            if (gender == 0 || gender == 2) putU32((shiny ? OFS_MALE_SHINY   : OFS_MALE)   + i, 1);
            if (gender == 1 || gender == 2) putU32((shiny ? OFS_FEMALE_SHINY : OFS_FEMALE) + i, 1);

            const int lang = bdspLangBit(pk->language());
            if (lang >= 0) putU32(OFS_LANGUAGE + i, getU32(OFS_LANGUAGE + i) | (1u << lang));

            // Per-form array, for the thirteen species that have one. Non-shiny and shiny are separate
            // arrays, the shiny one immediately after.
            const uint8_t form = pk->form();
            for (const auto& fa : FORM_ARRAYS) {
                if (fa.species != species) continue;
                if (form >= fa.count) break;      // a form this game's dex has no slot for
                const size_t ofs = fa.offset + (shiny ? static_cast<size_t>(fa.count) * 4 : 0)
                                 + static_cast<size_t>(form) * 4;
                putU32(ofs, 1);
                break;
            }
        };

        for (const auto& pk : party) registerMon(pk.get());
        for (const auto& box : boxes)
            for (const auto& pk : box) registerMon(pk.get());
    }

    void Trainer8BDSP::updateTrainerInfoBlock()
    {
        // Raw-buffer game: write straight into saveData (like updateItemBlock); recomputeHash() runs
        // after.
        if (saveData.size() < BDSP_MYSTATUS + 0x1A) return;
        setString(&saveData[BDSP_MYSTATUS], 0x1A, utf8ToUtf16(trainerName), 12);
        if (saveData.size() >= BDSP_MONEY + 4)
            writeUInt32LittleEndian(&saveData[BDSP_MONEY], money);
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
                // IsNew is an int32 at record 0x4. Only SET it for freshly-added items so the bag
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
