/**
 * Trainer8SWSH.cpp - Generation 8 Trainer Implementation
 *
 * This file implements the Trainer8SWSH class for Pokemon Sword/Shield save files.
 * Handles Gen 8-specific block parsing, Pokemon encryption/decryption, and
 * save file serialization.
 */
#include <algorithm>
#include <cstring>
#include <string>

#include "Trainer/Trainer8SWSH.h"
#include "Trainer/Inventory8SWSH.h"
#include "Pokemon/Pokemon8SWSH.h"
#include "Pokemon/SWSHDexTable.h"   // getSWSHDexEntry -- which of the three dexes a species lives in
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
         * These blocks hold the Isle of Armor / Crown Tundra Pokedex, and the DLC is detected from
         * whether either has RECORDED ANYTHING -- not from whether the block exists.
         *
         * Presence is not the signal, however much it looks like one. A patched v1.3.2 game
         * ALLOCATES both dex blocks in every save it creates, licence or no licence: a brand new
         * save on a console with no Expansion Pass carries a 10128-byte Isle of Armor dex and a
         * 10080-byte Crown Tundra dex, both entirely zero. Testing for presence therefore reports
         * Crown Tundra for everyone, which is what let the pickers offer Calyrex and the Crown
         * Tundra items to a base-game save.
         *
         * A dex only ever grows, so "contains a non-zero byte" is the honest question. Measured on
         * real saves: a fresh no-DLC save is 0 / 0 / 0 across Galar, Armor and Crown; after an hour
         * of base-game play Galar rises while both DLC dexes stay exactly 0; a played full-DLC save
         * shows 671 / 42 / 5. The DLC side of a genuine save is only a handful of bytes, so the test
         * has to be "any at all" rather than any kind of threshold.
         *
         * Known limit: a player who owns a DLC but has not yet seen a single Pokemon in it reads as
         * a lower tier until they do. That is the safe direction to be wrong in only because it is
         * self-correcting -- one DLC encounter fixes it -- whereas the presence test was wrong for
         * every no-DLC save permanently. PKHeX has the same gap and resolves it the same way its
         * own comment describes ("No DLC1 data allocated"), so there is no upstream answer to copy.
         *
         * Deliberately NOT applied to Scarlet/Violet or Z-A. Their triggers are a different shape --
         * S/V keys partly off a Blueberry Points COUNTER, which is legitimately zero on a save that
         * owns the Indigo Disk but has never earned any -- so the same rule there would invent a
         * false negative. Those need their own base-game save to measure before anything changes.
         */
        const auto hasRecordedData = [](const Save::Block& b) {
            for (uint8_t v : b.data) {
                if (v != 0) return true;
            }
            return false;
        };

        bool hasR2 = false;
        bool hasR1 = false;

        for (const auto& block : this->blocks) {
            if (block.key == SAVE_REVISION8_R2_SWSH && hasRecordedData(block)) {
                hasR2 = true;
            }
            if (block.key == SAVE_REVISION8_R1_SWSH && hasRecordedData(block)) {
                hasR1 = true;
            }
        }

        if (hasR2) {
            this->saveRevision = 2;
            this->saveRevisionString = "冠之雪原";
            this->gameVersionString = "v1.3";  // Crown Tundra requires v1.3.0+
        } else if (hasR1) {
            this->saveRevision = 1;
            this->saveRevisionString = "铠之孤岛";
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
         * 4. Write the game's encrypted blank into empty slots
         *
         * Empty party slots must be the game's encrypted "blank" (which DECRYPTS to species 0),
         * NOT literal zeros -- exactly like updateBoxBlock(). The game decrypts every slot and
         * validates it, so a zeroed slot decrypts to garbage and renders a BAD EGG. Measured on a
         * real Shield save: an empty party slot holds 331 non-zero bytes of 344 and decrypts to
         * species 0. It is not, and never was, a run of zeros.
         *
         * This path used to memset(0) and corrupted the party into Bad Eggs on ANY save, including
         * one that only touched a box -- the party is re-serialized unconditionally. updateBoxBlock
         * was fixed for this; the party writer was missed, which is why the boxes were fine and the
         * party was not.
         *
         * The party-count tail after the six slots (the block is 2068 bytes, not 6*344 = 2064) is
         * deliberately left untouched: parsePartyBlock treats an encrypted blank as occupied, so
         * empty slots load as species-0 "ghosts" that inflate party.size(). The save's own count is
         * authoritative, not party.size(). Same reasoning as Trainer8LA.
         */
        for (auto& block : blocks) {
            if (block.key == PARTY8_SWSH) {
                // Ensure the block data is large enough. Note this only ever GROWS the block, so a
                // real save's 4-byte count tail survives.
                size_t requiredSize = MAX_PARTY_SLOTS * SIZE_PARTY8_SWSH;
                if (block.data.size() < requiredSize) {
                    block.data.resize(requiredSize, 0);
                }

                // Which slots ALREADY read as empty, decided on the bytes in the file rather than
                // on party.size()? Those are left byte-for-byte alone below. The game's blank is
                // not a zeroed PK8 -- measured against a real save, a synthesized
                // encrypt(zeros, EC 0) differs from it by ~17 bytes per slot, and empty slots
                // differ from EACH OTHER too (stale bytes the game never cleared). So there is no
                // one canonical blank to stamp: the only way to leave an untouched party untouched
                // is to not write to it.
                //
                // "Decrypts to species 0" is NOT sufficient on its own, and the reason is a trap:
                // the cipher seeds its PRNG with the encryption constant, a zeroed slot has EC 0,
                // and the first PRNG word from seed 0 is 0x0000 -- so an all-zero slot decrypts to
                // species 0 as well, while its other 329 bytes are garbage the checksum rejects.
                // A slot wrecked by the old memset(0) would therefore look "already empty" and be
                // skipped, which is precisely the slot that needs repairing. A real blank always
                // carries non-zero bytes, so require that too.
                bool alreadyEmpty[MAX_PARTY_SLOTS] = {};
                for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) {
                    const size_t off = i * SIZE_PARTY8_SWSH;
                    if (off + SIZE_PARTY8_SWSH > block.data.size()) break;

                    bool anyNonZero = false;
                    for (size_t k = 0; k < SIZE_PARTY8_SWSH && !anyNonZero; ++k)
                        anyNonZero = (block.data[off + k] != 0);
                    if (!anyNonZero) continue;   // zeroed by an older build -> repair it below

                    std::vector<std::byte> enc(SIZE_PARTY8_SWSH);
                    std::memcpy(enc.data(), &block.data[off], SIZE_PARTY8_SWSH);
                    std::byte* dec = decryptArray8SWSH(
                        std::span<const std::byte>(enc.data(), SIZE_PARTY8_SWSH));
                    alreadyEmpty[i] = (readUInt16LittleEndian(
                        reinterpret_cast<const uint8_t*>(dec) + 0x08) == 0);
                    delete[] dec;
                }

                // For a slot that must BECOME empty (a Pokemon was removed), and for repairing a
                // slot an older build zeroed into a Bad Egg, synthesize a blank that decrypts to
                // species 0: an all-zero PK8 encrypted with EC 0 (== PKHeX's new PK8() BlankPKM).
                // Same construction as createBlankPokemon() and updateBoxBlock()'s fallback.
                std::vector<uint8_t> blankSlot;
                {
                    std::vector<std::byte> zero(SIZE_PARTY8_SWSH, std::byte{0});
                    std::byte* enc = encryptArray8SWSH(
                        std::span<const std::byte>(zero.data(), SIZE_PARTY8_SWSH), 0);
                    blankSlot.assign(reinterpret_cast<const uint8_t*>(enc),
                                     reinterpret_cast<const uint8_t*>(enc) + SIZE_PARTY8_SWSH);
                    delete[] enc;
                }

                // Writes an empty slot: keep the game's own bytes when they already read as empty,
                // otherwise lay down the blank. Never zeros -- that is the Bad Egg.
                const auto clearSlot = [&](size_t i, size_t offset) {
                    if (i < MAX_PARTY_SLOTS && alreadyEmpty[i]) return;
                    std::memcpy(&block.data[offset], blankSlot.data(), SIZE_PARTY8_SWSH);
                };

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
                        clearSlot(i, offset);   // never zeros -- see the header note
                    }
                }

                // Remaining trailing slots are empty too.
                for (size_t i = party.size(); i < MAX_PARTY_SLOTS; ++i) {
                    clearSlot(i, i * SIZE_PARTY8_SWSH);
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
                    // assign() from the pointer range, not resize()-then-fill. Both branches above
                    // are inlined into one body, and GCC cannot prove blankSlot is still empty
                    // here -- so it reads the resize as APPENDING 344 bytes onto the 344 the
                    // harvest branch allocated and reports a -Wstringop-overflow. Building the
                    // buffer in one step removes the pattern, and matches updatePartyBlock().
                    const uint8_t* encBytes = reinterpret_cast<const uint8_t*>(enc);
                    blankSlot.assign(encBytes, encBytes + SIZE_PARTY8_SWSH);
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

    // ---- Pokedex (three Zukan blocks) --------------------------------------------------------
    //
    // Sword/Shield have THREE dexes, one save block each with its own numbering: Galar, Isle of
    // Armor and Crown Tundra. A species belongs to exactly one (SWSHDexTable, generated from
    // PKHeX personal_swsh), and its entry is 0x30 bytes at (index - 1) * 0x30 inside that block.
    //
    // Entry layout (PKHeX Zukan8):
    //   0x00  four u64 SEEN regions -- not-shiny/male, not-shiny/female, shiny/male, shiny/female.
    //         Each bit is a FORM index (0-62); bit 63 is Gigantamax.
    //   0x20  u32 caught flags: bit0 Owned, bit1 OwnedGigantamax, bits2-14 languages,
    //         bits15-27 DisplayFormID, bit28 DisplayGigantamax, bits29-30 DisplayGender,
    //         bit31 DisplayShiny
    //   0x24  u32 battled count      0x28/0x2C reserved
    //
    // Same shape as Gen 3 and Let's Go: run over all of storage at save time, only ever set.
    namespace {
        constexpr size_t SWSH_OFS_CAUGHT = 0x20;
        constexpr size_t SWSH_SEEN_REGION = 8;   // one u64 per region

        // Language id -> dex language slot. Slot 6 (langID 6) is unused, so 7+ shift down by two.
        // Identical rule to Let's Go (PKHeX Zukan8.GetDexLangFlag).
        int swshLangSlot(uint8_t language) {
            if (language == 0 || language == 6 || language > 10) return -1;
            return (language >= 7) ? language - 2 : language - 1;
        }
    }

    void Trainer8SWSH::updatePokedexBlock()
    {
        // The three dex blocks. The DLC ones are absent (or empty) on a save without that DLC --
        // that is exactly how PKSE already detects the save revision -- so a missing block simply
        // means those species cannot be registered here.
        std::vector<uint8_t>* galar = nullptr;
        std::vector<uint8_t>* armor = nullptr;
        std::vector<uint8_t>* crown = nullptr;
        for (auto& block : blocks) {
            if      (block.key == SAVE_REVISION8_SWSH)    galar = &block.data;
            else if (block.key == SAVE_REVISION8_R1_SWSH) armor = &block.data;
            else if (block.key == SAVE_REVISION8_R2_SWSH) crown = &block.data;
        }
        if (!galar || galar->empty()) return;   // no base dex: nothing sane to write

        // Each dex block should be exactly (entry count) * 0x30. If one is not, the layout this code
        // assumes is wrong and every write into it would be silently dropped by the bounds check
        // below -- which reads as "the Pokedex just didn't update". Say so instead.
        struct { const char* name; const std::vector<uint8_t>* data; uint16_t count; } expect[] = {
            { "Galar", galar, ::Pokemon::SWSH_DEX_GALAR_COUNT },
            { "Armor", armor, ::Pokemon::SWSH_DEX_ARMOR_COUNT },
            { "Crown", crown, ::Pokemon::SWSH_DEX_CROWN_COUNT },
        };
        for (const auto& x : expect) {
            if (!x.data || x.data->empty()) continue;   // DLC the player doesn't have
            const size_t want = static_cast<size_t>(x.count) * ::Pokemon::SWSH_DEX_ENTRY_SIZE;
            if (x.data->size() != want) {
                logErrorToFile("Pokedex block size unexpected; registrations into it will be skipped",
                               (std::string(x.name) + ": got " + std::to_string(x.data->size())
                                + ", expected " + std::to_string(want)).c_str());
            }
        }

        auto registerMon = [&](const ::Pokemon::Pokemon* pk) {
            if (!pk || pk->isEgg()) return;
            const uint16_t species = pk->speciesID();
            if (species == 0) return;

            const ::Pokemon::SWSHDexEntry e = ::Pokemon::getSWSHDexEntry(species);
            if (e.dex == ::Pokemon::SWSHDex::None) return;   // not in any SWSH dex

            std::vector<uint8_t>* dex = nullptr;
            switch (e.dex) {
                case ::Pokemon::SWSHDex::Galar: dex = galar; break;
                case ::Pokemon::SWSHDex::Armor: dex = armor; break;
                case ::Pokemon::SWSHDex::Crown: dex = crown; break;
                default: return;
            }
            // A DLC dex the player does not own is absent or zero-length; skip rather than allocate
            // one, which would fabricate DLC data in a save that has none.
            if (!dex || dex->empty()) return;

            const size_t base = static_cast<size_t>(e.index - 1) * ::Pokemon::SWSH_DEX_ENTRY_SIZE;
            if (base + ::Pokemon::SWSH_DEX_ENTRY_SIZE > dex->size()) return;

            // SEEN: bit = form, inside the u64 for this gender/shiny combination. Forms past 62 have
            // no bit (63 is Gigantamax), so they are recorded against the base form rather than
            // spilling into the neighbouring region.
            uint8_t form = pk->form();
            if (form > 62) form = 0;
            const bool shiny = pk->isShiny(pk->id32(), pk->species());
            const int region = (pk->gender() == 1 ? 1 : 0) | (shiny ? 2 : 0);   // genderless -> male
            const size_t seenOfs = base + static_cast<size_t>(region) * SWSH_SEEN_REGION;
            (*dex)[seenOfs + (form >> 3)] |= static_cast<uint8_t>(1u << (form & 7));

            // CAUGHT flags (u32 at 0x20).
            const size_t cOfs = base + SWSH_OFS_CAUGHT;
            uint32_t flags = static_cast<uint32_t>((*dex)[cOfs])
                           | (static_cast<uint32_t>((*dex)[cOfs + 1]) << 8)
                           | (static_cast<uint32_t>((*dex)[cOfs + 2]) << 16)
                           | (static_cast<uint32_t>((*dex)[cOfs + 3]) << 24);
            const bool wasOwned = (flags & 1u) != 0;
            flags |= 1u;                                    // bit 0: owned
            const int lang = swshLangSlot(pk->language());
            if (lang >= 0) flags |= 1u << (2 + lang);        // bits 2-14: languages obtained

            // DisplayFormID (bits 15-27) picks which form the entry shows. Set it only for an entry
            // that was not owned before, so a species whose first catch is an alternate form displays
            // that form -- and an entry the player already has keeps whatever it was showing. Not
            // moved on later saves: registration walks storage, so "latest" would mean "last in box
            // order", which would change the dex display arbitrarily every save.
            if (!wasOwned) {
                flags = (flags & ~(0x1FFFu << 15)) | (static_cast<uint32_t>(form & 0x1FFF) << 15);
                if (shiny) flags |= 1u << 31;                // bit 31: display shiny
                if (pk->gender() == 1) flags |= 1u << 30;     // bits 29/30: display gender
                else                   flags |= 1u << 29;
            }
            (*dex)[cOfs]     = static_cast<uint8_t>(flags);
            (*dex)[cOfs + 1] = static_cast<uint8_t>(flags >> 8);
            (*dex)[cOfs + 2] = static_cast<uint8_t>(flags >> 16);
            (*dex)[cOfs + 3] = static_cast<uint8_t>(flags >> 24);
        };

        for (const auto& pk : party) registerMon(pk.get());
        for (const auto& box : boxes)
            for (const auto& pk : box) registerMon(pk.get());
    }

    void Trainer8SWSH::updateTrainerInfoBlock()
    {
        // OT name (0xB0) goes back into MyStatus8; money into its own block -- the same authoritative
        // places parse reads them, so edits take effect in-game. encrypt() re-hashes.
        for (auto& block : blocks) {
            if (block.key == MY_STATUS8_SWSH) {
                if (block.data.size() >= 0xB0 + 0x1A)
                    setString(&block.data[0xB0], 0x1A, utf8ToUtf16(trainerName), 12);
            } else if (block.key == MONEY8_SWSH) {
                if (block.data.size() >= 0x04 + 4)
                    writeUInt32LittleEndian(&block.data[0x04], money);
            }
        }
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
