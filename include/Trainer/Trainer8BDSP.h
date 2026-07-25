#ifndef TRAINER_TRAINER8BDSP_H
#define TRAINER_TRAINER8BDSP_H

#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon8BDSP.h"
#include "Encryption/Encryption8BDSP.h"
#include "Trainer/Inventory8BDSP.h"

using namespace Pokemon;
using namespace Encryption;

namespace Trainer {
    // ========================================
    // BDSP (SAV8BDSP) — a FLAT fixed-offset save, NOT SwishCrypto/SCBlocks.
    // ========================================
    // Brilliant Diamond / Shining Pearl store the save as one plaintext little-endian blob
    // guarded by a single whole-file MD5. Entities (PB8) are the SAME 0x148/0x158 Gen-8
    // format as Sword/Shield, held ENCRYPTED at fixed offsets. So Trainer8BDSP keeps the raw
    // save buffer and reads/writes fixed offsets instead of parsing keyed blocks.
    // All offsets are absolute into saveData (v1.0 layout). Ref: PKHeX SAV8BDSP.cs.
    constexpr size_t BDSP_PARTY_OFFSET   = 0x14098;   // 6 x SIZE_PARTY8_BDSP (0x158), encrypted
    constexpr size_t BDSP_PARTY_COUNT    = 0x148A8;   // u8 party count
    constexpr size_t BDSP_BOX_OFFSET     = 0x14EF4;   // 40 boxes x 30 slots x SIZE_PARTY8_BDSP
    constexpr size_t BDSP_BOXNAME_OFFSET = 0x148AA;   // 40 x 0x22 bytes (17 UTF-16 chars each)
    constexpr size_t BDSP_BOXNAME_LENGTH = 0x22;      // bytes per box name; last slot is the terminator
    constexpr size_t BDSP_CURRENTBOX     = 0x14EC8;   // u8 current box (BoxLayout8b.CurrentBox, = BOXNAME + 0x61E)
    constexpr size_t BDSP_MYSTATUS       = 0x79BB4;   // MyStatus8b; OT name (0x1A bytes) starts here
    constexpr size_t BDSP_ID32           = 0x79BD0;   // TID16 @ 0x79BD0 / SID16 @ 0x79BD2
    constexpr size_t BDSP_MONEY          = 0x79BD4;   // u32 (max 999,999)
    constexpr size_t BDSP_GENDER         = 0x79BD8;   // Male byte (1 = male, 0 = female)
    constexpr size_t BDSP_ROMCODE        = 0x79BDF;   // BD = 0 / SP = 1
    constexpr size_t BDSP_HASH_OFFSET    = 0xE9818;   // MD5 (16 bytes) over the whole file, hash zeroed
    constexpr size_t BDSP_ITEM_BASE      = 0x0563C;   // MyItem8b: item i's 0x10 record at BASE + i*0x10
    constexpr size_t BDSP_BOX_COUNT      = 40;

    class Trainer8BDSP : public Trainer {
    private:
        std::vector<uint8_t> saveData;    // the full flat save (BDSP has no SCBlocks)
        std::vector<uint8_t> m_boxBlank;  // the game's encrypted blank box slot (for empty-slot writes)

        void parseMyStatus();
        void parseParty();
        void parseBoxes();
        void parseBoxNames();
        void parseItems();

    public:
        explicit Trainer8BDSP(std::vector<uint8_t> data);

        void updatePartyBlock() override;
        void updateBoxBlock() override;
        void updateBoxNameBlock() override;
        void updateCurrentBoxBlock() override;   // u8 at BDSP_CURRENTBOX (flat offset, no SCBlock)
        bool supportsBoxNames() const noexcept override { return true; }
        size_t getMaxBoxNameLength() const noexcept override { return BDSP_BOXNAME_LENGTH / 2 - 1; }
        void updateItemBlock() override;   // BDSP items deferred (no-op stub)
        bool itemsAreIdIndexed() const override { return true; }   // count at BDSP_ITEM_BASE + itemId*stride

        // Species-0, checksum-valid blank PB8 (mirrors updateBoxBlock()'s encrypted-blank fallback:
        // zeroed SIZE_PARTY8_BDSP buffer -> encryptArray8BDSP(seed 0) -> Pokemon8BDSP). Creator seed.
        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        size_t getBoxCount() const noexcept override { return BDSP_BOX_COUNT; }
        size_t getSlotsPerBox() const noexcept override { return BOX_SLOTS; }
        size_t getPartySize() const noexcept override { return party.size(); }
        GameVersion getGameGroup() const noexcept override { return GameVersion::BDSP; }

        /// Full flat save buffer (for writing back + MD5 rehash).
        const std::vector<uint8_t>& getSaveData() const noexcept { return saveData; }
        /// Recompute the whole-file MD5 into BDSP_HASH_OFFSET (call before writing to disk).
        void recomputeHash();
    };
}

#endif
