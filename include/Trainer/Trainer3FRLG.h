#ifndef TRAINER_TRAINER3_FRLG_H
#define TRAINER_TRAINER3_FRLG_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Trainer/Trainer.h"
#include "Trainer/Inventory3FRLG.h"
#include "Pokemon/Pokemon3FRLG.h"
#include "Encryption/Encryption3FRLG.h"

namespace Trainer {
    // ========================================
    // SAV3FRLG — GBA FireRed/LeafGreen 128 KiB save.
    // ========================================
    // Two slots (A@0x0000, B@0xE000) of 14 rotated 0x1000 sectors. Each sector carries its logical id
    // in a footer (+0xFF4) and a save counter (+0xFFC); the ACTIVE slot is the one whose id-0 sector has
    // the greater counter. Logical blocks are concatenations of their sectors' first 0xF80 data bytes:
    //   Small   = id 0        (trainer info + security key)
    //   Large   = id 1..4     (party / items / money)
    //   Storage = id 5..13    (14 PC boxes; an 80-byte box mon may STRADDLE a sector boundary)
    // Money + bag item-counts are XOR-obfuscated with the security key (Small+0xF20). Trainer3FRLG keeps
    // the full raw save and maps logical block offsets -> absolute file offsets through the active slot's
    // sector table, so straddling is handled transparently. Ref: docs/ROADMAP_TO_V1.md App. A (PKHeX SAV3FRLG).
    constexpr size_t FRLG_SAVE_SIZE   = 0x20000;
    constexpr size_t FRLG_SLOT_A      = 0x00000;
    constexpr size_t FRLG_SLOT_B      = 0x0E000;
    constexpr size_t FRLG_SECTOR_SIZE = 0x1000;
    constexpr size_t FRLG_SECTOR_DATA = 0xF80;   // usable data bytes per sector (footer follows)
    constexpr size_t FRLG_SECTORS     = 14;      // sectors per slot
    constexpr size_t FRLG_BOX_COUNT   = 14;
    constexpr size_t FRLG_BOX_SLOTS   = 30;
    // Box names: 14 x 9 bytes at Storage+0x8344. Gen 3 text, 8 usable chars + a 0xFF terminator
    // (PKHeX SAV3.COUNT_BOXNAME = 8 + 1).
    constexpr size_t FRLG_BOX_NAME_OFFSET = 0x8344;
    constexpr size_t FRLG_BOX_NAME_BYTES  = 9;
    constexpr size_t FRLG_BOX_NAME_CHARS  = 8;

    class Trainer3FRLG : public Trainer {
    private:
        std::vector<uint8_t> saveData;              // full raw save (0x20000)
        std::string m_fileName;                     // discovered on-disk name (for write-back)
        size_t   m_slotBase = 0;                    // active slot base offset
        size_t   m_sectorOfs[FRLG_SECTORS] = {0};   // absolute offset of the sector holding logical id i
        uint32_t m_key = 0;                         // security key (Small+0xF20)
        bool     m_valid = false;

        // Logical-block base sector ids.
        static constexpr int SMALL_ID   = 0;
        static constexpr int LARGE_ID   = 1;   // ids 1..4
        static constexpr int STORAGE_ID = 5;   // ids 5..13

        void selectActiveSlot();
        void parseTrainer();
        void parseParty();
        void parseBoxes();
        void parseBoxNames();
        void parseItems();

        // Map a logical offset within a block (whose first sector is logical id `blockBaseId`) to the
        // absolute file offset, honoring sector rotation; read/write cross sector boundaries byte-wise.
        void readBlock(int blockBaseId, size_t logical, uint8_t* dst, size_t len) const;
        void writeBlock(int blockBaseId, size_t logical, const uint8_t* src, size_t len);
        // Decrypt a PK3 record of `size` bytes at a logical block offset into a Pokemon3FRLG.
        std::unique_ptr<::Pokemon::Pokemon> readMon(int blockBaseId, size_t logical, size_t size) const;

    public:
        // `fileName` is the save's on-disk basename in the backup dir (e.g. "FireRed_e.sav"); it is
        // reused verbatim for the ModifiedSave write-back so we round-trip whatever name we loaded.
        explicit Trainer3FRLG(std::vector<uint8_t> data, std::string fileName);

        void updatePartyBlock() override;
        void updateBoxBlock() override;
        void updateBoxNameBlock() override;
        void updateCurrentBoxBlock() override;   // u8 at Storage logical offset 0
        bool supportsBoxNames() const noexcept override { return true; }
        size_t getMaxBoxNameLength() const noexcept override { return FRLG_BOX_NAME_CHARS; }
        bool canStoreBoxName(const std::string& name) const override;
        void updateItemBlock() override;

        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        size_t getBoxCount() const noexcept override { return FRLG_BOX_COUNT; }
        size_t getSlotsPerBox() const noexcept override { return FRLG_BOX_SLOTS; }
        size_t getPartySize() const noexcept override { return party.size(); }
        GameVersion getGameGroup() const noexcept override { return GameVersion::FRLG; }

        bool isValid() const noexcept { return m_valid; }
        const std::string& fileName() const noexcept { return m_fileName; }
        const std::vector<uint8_t>& getSaveData() const noexcept { return saveData; }
        uint32_t securityKey() const noexcept { return m_key; }

        // Recompute all 14 active-slot sector checksums into their footers (call before writing to disk).
        void finalizeChecksums();
    };
}

#endif  // TRAINER_TRAINER3_FRLG_H
