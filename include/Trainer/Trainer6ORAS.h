/**
 * ONE CONTAINER, FOUR GAMES, TWO CHECKSUM ALGORITHMS. The block layout is generated into
 * BlockEntry3DSs.h; everything a game-specific class supplies here is an index into that table plus
 * the answer to "which CRC?".
 *
 * The file ends with a BEEF metadata chunk in its last 0x200 bytes, and block `id`'s checksum
 * lives at `fileSize - 0x200 + 0x14 + id*8 + 6`. Refreshing a block means recomputing its CRC over
 * its own data and storing it there -- there is no mirror, no counter and no partition, so an
 * unedited round trip is exact by construction.
 *
 * THE ONE THING THAT WILL BITE: GEN 6 USES CRC16-CCITT, GEN 7 USES CRC16Invert. They are different
 * algorithms of the same width over the same bytes, so using the wrong one produces a perfectly
 * well-formed 16-bit number that the game rejects. `blockCrc()` is the hook, and it is the only
 * behavioural difference between the two generations' containers.
 *
 * Three layout facts that differ from every earlier generation:
 *
 *  - THERE ARE 31 BOXES IN GEN 6 AND 32 IN GEN 7, not 24 or 18.
 *  - THE PARTY COUNT IS AFTER THE RECORDS, at `party + 6*SIZE_PARTY`, not before them.
 *  - BOX NAMES ARE 0x22 BYTES (17 UTF-16 units) and the box layout's fields sit at different
 *    offsets in the two generations -- Gen 6's current box is at 0x43F, Gen 7's at 0x5E3.
 */
#ifndef TRAINER_TRAINER6_ORAS_H
#define TRAINER_TRAINER6_ORAS_H

#include <cstdint>
#include <string>
#include <vector>

#include "Trainer/Trainer.h"
#include "Trainer/Inventory6ORAS.h"
#include "Pokemon/Pokemon6ORAS.h"
#include "Trainer/Blocks6ORAS.h"
#include "Pokemon/Pokemon6XY.h"
#include "Pokemon/Pokemon7SM.h"

namespace Trainer
{

    class Trainer6ORAS final : public Trainer
    {
    public:
        Trainer6ORAS(std::vector<uint8_t> raw, std::string path);

        const std::vector<uint8_t> &serialize();
        const std::vector<uint8_t> &getSaveData() const noexcept { return saveData; }
        const std::string &fileName() const noexcept { return savePath; }
        bool isValid() const noexcept { return valid; }

        void updatePartyBlock() override;
        void updateBoxBlock() override;
        void updateItemBlock() override;
        void updateTrainerInfoBlock() override;
        void updateBoxNameBlock() override;
        void updateCurrentBoxBlock() override;
        std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const override;

        GameVersion getGameGroup() const noexcept override { return GameVersion::ORAS; }
        size_t getBoxCount() const noexcept override { return 31; }

        size_t getSlotsPerBox() const noexcept override { return 30; }
        size_t getPartySize() const noexcept override { return party.size(); }
        uint32_t getMaxMoney() const noexcept override { return 9999999; }
        size_t getMaxTrainerNameLength() const noexcept override { return 12; }
        /// PKHeX TrainerNameVerifier: five digits from Gen 6 on, four before.
        int getMaxTrainerNameDigits() const noexcept override { return 5; }
        bool supportsBoxNames() const noexcept override { return true; }
        /// The field is 0x22 bytes = 17 UTF-16 units, so 16 characters plus a terminator.
        size_t getMaxBoxNameLength() const noexcept override { return 16; }
        bool itemsAreIdIndexed() const noexcept override { return false; }
        bool compactStorage() override { return false; }
        bool canStoreBoxName(const std::string &name) const override;

        // Concrete here rather than virtual: this class serves exactly one layout.
        const BlockEntry3DS *blocks() const noexcept { return BLOCKS_6ORAS; }
        size_t blockCount() const noexcept { return BLOCK_COUNT_6ORAS; }
        size_t fileSize() const noexcept { return SAVE_SIZE_6ORAS; }
        size_t blkItem() const noexcept { return BLOCK_ITEM_6ORAS; }
        size_t blkBoxLayout() const noexcept { return BLOCK_BOX_LAYOUT_6ORAS; }
        size_t blkStatus() const noexcept { return BLOCK_STATUS_6ORAS; }
        size_t blkMisc() const noexcept { return BLOCK_MISC_6ORAS; }
        size_t blkParty() const noexcept { return BLOCK_PARTY_6ORAS; }
        size_t blkBox() const noexcept { return BLOCK_BOX_6ORAS; }
        uint16_t blockCrc(const uint8_t *bytes, size_t byteCount) const noexcept;


        /// Size + the BEEF magic + the metadata chunk's own block lengths matching this game's
        /// table. PKHeX stops at size + magic; checking the lengths as well is what makes a
        /// mis-sized or foreign `main` fail here instead of parsing into nonsense.
        static bool detectGen67(const std::vector<uint8_t> &bytes, size_t size,
                                const BlockEntry3DS *tbl, size_t byteCount) noexcept;

        static bool detect(const std::vector<uint8_t> &bytes) noexcept
        {
            return detectGen67(bytes, SAVE_SIZE_6ORAS, BLOCKS_6ORAS, BLOCK_COUNT_6ORAS);
        }

        /// Which of Omega Ruby and Alpha Sapphire this save is, read from MyStatus's own
        /// game byte (+0x04).
        GameVersion getGameVersion() const noexcept override;

    protected:
        std::vector<uint8_t> saveData;
        std::string savePath;
        bool valid = false;
        /// A verbatim copy of the first empty box slot found at load: this game's own blank
        /// record, used when a slot has to be cleared. See parseBoxes().
        std::vector<uint8_t> emptyTemplate;
        /// The pouch slot each parsed item came out of. Gen 6 pouches are positional, not packed,
        /// so an item that was in the bag when it was read is written back to its own slot.
        std::vector<std::vector<ItemOrigin>> itemSlot;

        const BlockEntry3DS &blk(size_t blockIndex) const noexcept { return blocks()[blockIndex]; }
        uint8_t *at(size_t blockIndex) noexcept { return &saveData[blk(blockIndex).offset]; }
        const uint8_t *at(size_t blockIndex) const noexcept { return &saveData[blk(blockIndex).offset]; }
        size_t entitySize(bool partySlot) const noexcept;
        std::unique_ptr<::Pokemon::Pokemon> makeEntity(const uint8_t *bytes, size_t byteCount) const;

        void init();
        void writeChecksums() noexcept;
        void parse();

    private:
        void parseTrainer();
        void parseParty();
        void parseBoxes();
        void parseItems();
        void parseBoxNames();
    };

}

#endif
