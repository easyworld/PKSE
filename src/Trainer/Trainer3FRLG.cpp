/**
 * Trainer3FRLG.cpp - GBA FireRed/LeafGreen (SAV3FRLG) trainer implementation.
 *
 * A 128 KiB save of two slots, each 14 rotated 0x1000 sectors (see Trainer3FRLG.h). We keep the raw
 * save buffer and address logical blocks (Small/Large/Storage) through the active slot's sector table,
 * so box mons that straddle a sector boundary are handled byte-wise. Entities (PK3) are 80 B stored /
 * 100 B party, encrypted (XOR + PID%24 shuffle). Money + bag item-counts are XOR'd with the security
 * key. On save we re-encrypt the party/boxes/items in place and recompute every sector checksum.
 *
 * Offsets validated byte-for-byte against a real FireRed save. Ref: docs/ROADMAP_TO_V1.md App. A.
 */
#include <algorithm>
#include <cstring>

#include "Trainer/Trainer3FRLG.h"
#include "Utils/Gen3Text.h"        // the Gen 3 character set, shared with Pokemon3FRLG + Convert
#include "Utils/HelperUtilities.h"
#include "Utils/Logger.h"

using namespace Utils;
using namespace Pokemon;
using namespace Encryption;

namespace Trainer {

    namespace {
        // ---- Gen 3 English text for the trainer + box names (table: Utils/Gen3Text.h) ----
        // Decodes through UTF-16 and hands back UTF-8, so the accents and the ♀/♂ a Gen 3 name may
        // legitimately contain survive into a std::string. Going straight to narrow chars is what
        // silently dropped them before -- none of them fit in one.
        std::string g3Decode(const uint8_t* p, size_t maxLen) {
            std::u16string wide;
            for (size_t i = 0; i < maxLen; ++i) {
                const uint8_t b = p[i];
                if (b == Utils::GEN3_TERMINATOR) break;
                if (const char16_t c = Utils::gen3ToChar(b)) wide += c;   // 0 = no glyph -> skip it
            }
            // trim trailing spaces (Gen 3 pads short names with spaces)
            while (!wide.empty() && wide.back() == u' ') wide.pop_back();
            return Utils::utf16ToUtf8(wide);
        }

        /**
         * Inverse of g3Decode. Returns FALSE if any character has no Gen 3 representation, so the
         * caller can refuse the name outright rather than silently storing a mangled one -- the
         * Switch keyboard will happily produce accents and emoji that this table cannot express.
         *
         * Writes the text plus a single 0xFF terminator and **touches nothing after it**, so `out`
         * must arrive holding the bytes currently in the save.
         *
         * That tail matters more than it looks. The round-trip harness showed a real FireRed
         * save carries a MIX of 0x00 and 0xFF after the terminator -- the game writes a name and
         * leaves whatever was already there. Clearing to 0xFF drifted 33 bytes; clearing to 0x00
         * drifted 19. Only preserving the tail reproduces the file, and it is the safer rule anyway:
         * don't rewrite bytes you have no reason to touch. Decoding never noticed either way, since
         * g3Decode stops at the terminator -- which is precisely why only a byte compare found it.
         *
         * `in` is UTF-8 and is decoded before mapping. Walking it as raw bytes instead used to reject
         * every multi-byte character on the lead byte alone, which quietly refused names Gen 3 can in
         * fact store -- ♀, ♂ and the accented letters all have real bytes in its table.
         *
         * maxChars counts Gen 3 bytes, i.e. glyphs, not UTF-8 code units, so a name of accents still
         * measures against the field the way the player sees it.
         */
        bool g3Encode(const std::string& in, uint8_t* out, size_t bytes, size_t maxChars) {
            size_t n = 0;
            for (const char16_t c : Utils::utf8ToUtf16(in)) {
                if (n >= maxChars || n >= bytes) break;
                const uint8_t b = Utils::charToGen3(c);
                if (b == Utils::GEN3_TERMINATOR) return false;   // no Gen 3 glyph for this character
                out[n++] = b;
            }
            if (n < bytes) out[n] = Utils::GEN3_TERMINATOR;   // terminator; everything past it is left alone
            return true;
        }

        // Build a 100-byte ENCRYPTED party record from an entity, computing the party-only battle stats
        // (0x50-0x63) so a mon promoted from a box (80 B, no stats) serializes correctly. Caller writes 100 B.
        void buildPartyRecord(::Pokemon::Pokemon& pk, uint8_t out[100]) {
            uint8_t buf[100];
            std::memset(buf, 0, sizeof(buf));
            const size_t n = std::min<size_t>(pk.getDataSize(), 100);
            std::memcpy(buf, pk.getData().data(), n);           // canonical header + G/A/E/M (+ stats if party)
            buf[0x54] = pk.level();
            writeUInt16LittleEndian(buf + 0x56, pk.statHPMax());  // current HP = max (heal on write)
            writeUInt16LittleEndian(buf + 0x58, pk.statHPMax());  // max HP
            writeUInt16LittleEndian(buf + 0x5A, pk.statATK());
            writeUInt16LittleEndian(buf + 0x5C, pk.statDEF());
            writeUInt16LittleEndian(buf + 0x5E, pk.statSPE());
            writeUInt16LittleEndian(buf + 0x60, pk.statSPA());
            writeUInt16LittleEndian(buf + 0x62, pk.statSPD());
            std::byte* enc = encryptArray3FRLG(
                std::span<const std::byte>(reinterpret_cast<const std::byte*>(buf), 100));
            std::memcpy(out, enc, 100);
            delete[] enc;
        }

        // Build an 80-byte ENCRYPTED stored record (box format; the party-only battle stats are dropped).
        void buildBoxRecord(::Pokemon::Pokemon& pk, uint8_t out[80]) {
            uint8_t buf[80];
            std::memset(buf, 0, sizeof(buf));
            const size_t n = std::min<size_t>(pk.getDataSize(), 80);
            std::memcpy(buf, pk.getData().data(), n);
            std::byte* enc = encryptArray3FRLG(
                std::span<const std::byte>(reinterpret_cast<const std::byte*>(buf), 80));
            std::memcpy(out, enc, 80);
            delete[] enc;
        }
    }

    Trainer3FRLG::Trainer3FRLG(std::vector<uint8_t> data, std::string fileName)
        : Trainer(std::vector<Block>{}), saveData(std::move(data)), m_fileName(std::move(fileName))
    {
        // Always leave the containers in a consistent shape, even for a bad save.
        boxes.clear();  boxes.resize(FRLG_BOX_COUNT);
        items.clear();  items.resize(POUCH_COUNT3_FRLG);
        boxNames.clear();
        for (size_t b = 0; b < FRLG_BOX_COUNT; ++b) boxNames.push_back("盒子 " + std::to_string(b + 1));

        if (saveData.size() < FRLG_SAVE_SIZE) {
            logErrorToFile("FRLG save too small (need 0x20000 bytes)");
            return;
        }
        selectActiveSlot();
        if (!m_valid) {
            logErrorToFile("FRLG save missing one or more sector ids (0..13) in the active slot");
            return;
        }
        parseTrainer();
        parseParty();
        parseBoxes();
        parseBoxNames();
        parseItems();
    }

    void Trainer3FRLG::selectActiveSlot() {
        auto slotCounter = [&](size_t base) -> uint32_t {
            for (size_t s = 0; s < FRLG_SECTORS; ++s) {
                const size_t off = base + s * FRLG_SECTOR_SIZE;
                if (readUInt16LittleEndian(&saveData[off + 0xFF4]) == 0)
                    return readUInt32LittleEndian(&saveData[off + 0xFFC]);
            }
            return 0;
        };
        const uint32_t ca = slotCounter(FRLG_SLOT_A);
        const uint32_t cb = slotCounter(FRLG_SLOT_B);
        // An unwritten slot's counter is 0xFFFFFFFF (erased sentinel) and must lose; otherwise greater wins.
        bool aWins;
        if (ca == 0xFFFFFFFFu && cb != 0xFFFFFFFFu) aWins = false;
        else if (cb == 0xFFFFFFFFu && ca != 0xFFFFFFFFu) aWins = true;
        else aWins = (ca >= cb);
        m_slotBase = aWins ? FRLG_SLOT_A : FRLG_SLOT_B;

        // Resolve the rotated sector table: m_sectorOfs[id] = absolute offset of the sector carrying id.
        bool seen[FRLG_SECTORS] = {false};
        int found = 0;
        for (size_t s = 0; s < FRLG_SECTORS; ++s) {
            const size_t off = m_slotBase + s * FRLG_SECTOR_SIZE;
            const uint16_t id = readUInt16LittleEndian(&saveData[off + 0xFF4]);
            if (id < FRLG_SECTORS && !seen[id]) { m_sectorOfs[id] = off; seen[id] = true; ++found; }
        }
        m_valid = (found == static_cast<int>(FRLG_SECTORS));

        char buf[128];
        snprintf(buf, sizeof(buf), "FRLG active slot @0x%05zX (counter A=%u B=%u)", m_slotBase, ca, cb);
        logInfoToFile(buf);
    }

    void Trainer3FRLG::readBlock(int blockBaseId, size_t logical, uint8_t* dst, size_t len) const {
        for (size_t i = 0; i < len; ++i) {
            const size_t L = logical + i;
            const int sec = blockBaseId + static_cast<int>(L / FRLG_SECTOR_DATA);
            const size_t within = L % FRLG_SECTOR_DATA;
            if (sec < 0 || sec >= static_cast<int>(FRLG_SECTORS)) { dst[i] = 0; continue; }
            dst[i] = saveData[m_sectorOfs[sec] + within];
        }
    }

    void Trainer3FRLG::writeBlock(int blockBaseId, size_t logical, const uint8_t* src, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            const size_t L = logical + i;
            const int sec = blockBaseId + static_cast<int>(L / FRLG_SECTOR_DATA);
            const size_t within = L % FRLG_SECTOR_DATA;
            if (sec < 0 || sec >= static_cast<int>(FRLG_SECTORS)) continue;
            saveData[m_sectorOfs[sec] + within] = src[i];
        }
    }

    std::unique_ptr<::Pokemon::Pokemon> Trainer3FRLG::readMon(int blockBaseId, size_t logical, size_t size) const {
        uint8_t tmp[100];
        if (size > sizeof(tmp)) size = sizeof(tmp);
        readBlock(blockBaseId, logical, tmp, size);
        return std::make_unique<Pokemon3FRLG>(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(tmp), size));
    }

    void Trainer3FRLG::parseTrainer() {
        const size_t sm = m_sectorOfs[SMALL_ID];
        this->trainerName = g3Decode(&saveData[sm + 0x00], 7);
        this->TID16 = readUInt16LittleEndian(&saveData[sm + 0x0A]);
        this->SID16 = readUInt16LittleEndian(&saveData[sm + 0x0C]);
        this->trainerGender = saveData[sm + 0x08] & 1;   // 0x08: player gender (0=M, 1=F)
        this->ID32  = readUInt32LittleEndian(&saveData[sm + 0x0A]);
        this->TID   = this->TID16;   // Gen 3's visible trainer ID is the raw 16-bit TID
        this->SID   = this->SID16;
        this->m_key = readUInt32LittleEndian(&saveData[sm + 0xF20]);

        uint8_t moneyBuf[4];
        readBlock(LARGE_ID, 0x290, moneyBuf, 4);
        this->money = readUInt32LittleEndian(moneyBuf) ^ m_key;

        logInfoToFile("Parsed FRLG Trainer Name", this->trainerName.c_str());
    }

    void Trainer3FRLG::parseParty() {
        party.clear();
        uint8_t count = 0;
        readBlock(LARGE_ID, 0x034, &count, 1);
        if (count > MAX_PARTY_SLOTS) count = MAX_PARTY_SLOTS;
        for (uint8_t i = 0; i < count; ++i) {
            auto mon = readMon(LARGE_ID, 0x038 + static_cast<size_t>(i) * 100, 100);
            if (mon->speciesID() != 0) party.push_back(std::move(mon));
        }
    }

    void Trainer3FRLG::parseBoxes() {
        boxes.clear();
        boxes.resize(FRLG_BOX_COUNT);
        for (size_t b = 0; b < FRLG_BOX_COUNT; ++b) {
            for (size_t s = 0; s < FRLG_BOX_SLOTS; ++s) {
                const size_t idx = b * FRLG_BOX_SLOTS + s;
                const size_t logical = 0x0004 + idx * 80;   // Storage: [0]=current-box byte, mons at +4
                auto mon = readMon(STORAGE_ID, logical, 80);
                if (mon->speciesID() != 0) boxes[b][s] = std::move(mon);
            }
        }
    }

    bool Trainer3FRLG::canStoreBoxName(const std::string& name) const {
        // Gen 3's table is wide (247 bytes: letters, digits, accents, ♀/♂, common punctuation) but
        // it is not Unicode, so whatever the Switch keyboard adds past it must be refused up front
        // rather than dropped on write.
        uint8_t scratch[FRLG_BOX_NAME_BYTES];
        return g3Encode(name, scratch, FRLG_BOX_NAME_BYTES, FRLG_BOX_NAME_CHARS);
    }

    void Trainer3FRLG::updateBoxNameBlock() {
        // Inverse of parseBoxNames: 14 names of 9 bytes at Storage+0x8344, Gen 3 text, 0xFF
        // terminated. Must run BEFORE finalizeChecksums() -- the names sit inside the checksummed
        // sector data, so writing them afterwards would leave every storage sector's checksum stale
        // and the game would reject the save.
        for (size_t b = 0; b < FRLG_BOX_COUNT && b < boxNames.size(); ++b) {
            if (!isBoxNameDirty(b)) continue;   // never persist a display default
            uint8_t nameBuf[FRLG_BOX_NAME_BYTES];
            // Seed with what's already in the save: g3Encode only writes the text and terminator,
            // deliberately leaving the bytes past it untouched (see g3Encode).
            readBlock(STORAGE_ID, FRLG_BOX_NAME_OFFSET + b * FRLG_BOX_NAME_BYTES,
                      nameBuf, FRLG_BOX_NAME_BYTES);
            // A name the Gen 3 table can't express is skipped rather than written mangled. The UI
            // validates before getting here, so this is a backstop, not the primary check.
            if (!g3Encode(boxNames[b], nameBuf, FRLG_BOX_NAME_BYTES, FRLG_BOX_NAME_CHARS)) continue;
            writeBlock(STORAGE_ID, FRLG_BOX_NAME_OFFSET + b * FRLG_BOX_NAME_BYTES,
                       nameBuf, FRLG_BOX_NAME_BYTES);
        }
    }

    // ---- Pokedex ----------------------------------------------------------------------------
    //
    // Gen 3 keeps two flag arrays indexed by (national dex number - 1): CAUGHT once, and SEEN in
    // **three** separate places -- one in the Small block and two mirrors in the Large block. The game
    // cross-checks them, so writing only the first leaves a dex that disagrees with itself. Offsets are
    // PKHeX's (SAV3.SetSeen/SetCaught + SaveBlock3LargeFRLG.SeenOffset2/3).
    //
    // 386 species need 49 bytes of flags. Read/modify/write the whole array once per location rather
    // than a byte per Pokemon, and only ever OR bits in -- never clear one (see updatePokedexBlock).
    void Trainer3FRLG::updatePokedexBlock() {
        constexpr size_t DEX_SMALL       = 0x18;              // Pokedex struct, Small block
        constexpr size_t OFS_CAUGHT      = DEX_SMALL + 0x10;  // 0x28
        constexpr size_t OFS_SEEN_SMALL  = DEX_SMALL + 0x44;  // 0x5C
        constexpr size_t OFS_PID_UNOWN   = DEX_SMALL + 0x04;  // 0x1C -- decides the letter the dex shows
        constexpr size_t OFS_PID_SPINDA  = DEX_SMALL + 0x08;  // 0x20 -- likewise, the spot pattern
        constexpr size_t OFS_SEEN_LARGE2 = 0x5F8;
        constexpr size_t OFS_SEEN_LARGE3 = 0x3A18;
        constexpr size_t FLAG_BYTES      = 49;                // ceil(386 / 8)
        constexpr uint16_t MAX_SPECIES3  = 386;

        uint8_t caught[FLAG_BYTES], seen[FLAG_BYTES], mirror2[FLAG_BYTES], mirror3[FLAG_BYTES];
        readBlock(SMALL_ID, OFS_CAUGHT,      caught,  FLAG_BYTES);
        readBlock(SMALL_ID, OFS_SEEN_SMALL,  seen,    FLAG_BYTES);
        readBlock(LARGE_ID, OFS_SEEN_LARGE2, mirror2, FLAG_BYTES);
        readBlock(LARGE_ID, OFS_SEEN_LARGE3, mirror3, FLAG_BYTES);
        // Start from the UNION of all three copies. They are supposed to agree, but if they have drifted
        // (a half-written save, an older tool) then rebuilding the mirrors from the Small array alone
        // would delete whatever the mirrors knew and the Small one didn't. Merging can only add.
        for (size_t i = 0; i < FLAG_BYTES; ++i) seen[i] |= static_cast<uint8_t>(mirror2[i] | mirror3[i]);

        // Unown's letter and Spinda's spots are drawn in the dex from a stored PID, not from the entity.
        // The games record the FIRST one seen and never revise it, so only write when the species is not
        // already flagged -- otherwise every save would repaint the dex from whatever is in box order.
        auto firstSeen = [&](uint16_t species) {
            const int bit = species - 1;
            return (seen[bit >> 3] & (1u << (bit & 7))) == 0;
        };

        auto registerMon = [&](const ::Pokemon::Pokemon* pk) {
            if (!pk) return;
            const uint16_t species = pk->speciesID();
            if (species == 0 || species > MAX_SPECIES3) return;
            if (pk->isEgg()) return;          // an egg is not seen or owned until it hatches
            if (species == 201 || species == 327) {   // Unown / Spinda
                if (firstSeen(species)) {
                    uint8_t pidLE[4];
                    const uint32_t pid = pk->pid();
                    pidLE[0] = static_cast<uint8_t>(pid);        pidLE[1] = static_cast<uint8_t>(pid >> 8);
                    pidLE[2] = static_cast<uint8_t>(pid >> 16);  pidLE[3] = static_cast<uint8_t>(pid >> 24);
                    writeBlock(SMALL_ID, species == 201 ? OFS_PID_UNOWN : OFS_PID_SPINDA, pidLE, 4);
                }
            }
            const int bit = species - 1;
            caught[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
            seen[bit >> 3]   |= static_cast<uint8_t>(1u << (bit & 7));
        };

        for (const auto& pk : party) registerMon(pk.get());
        for (const auto& box : boxes)
            for (const auto& pk : box) registerMon(pk.get());

        writeBlock(SMALL_ID, OFS_CAUGHT,     caught, FLAG_BYTES);
        writeBlock(SMALL_ID, OFS_SEEN_SMALL, seen,   FLAG_BYTES);
        // The two Large-block mirrors carry the SEEN array only; there is no second caught array.
        writeBlock(LARGE_ID, OFS_SEEN_LARGE2, seen, FLAG_BYTES);
        writeBlock(LARGE_ID, OFS_SEEN_LARGE3, seen, FLAG_BYTES);
    }

    void Trainer3FRLG::updateCurrentBoxBlock() {
        // Current box is one byte at Storage logical offset 0 (PKHeX SAV3.CurrentBox). Like the box
        // names it lives in the checksummed sector data, so this must run BEFORE finalizeChecksums().
        uint8_t cb = static_cast<uint8_t>(currentBox);
        writeBlock(STORAGE_ID, 0, &cb, 1);
    }

    void Trainer3FRLG::parseBoxNames() {
        boxNames.clear();
        for (size_t b = 0; b < FRLG_BOX_COUNT; ++b) {
            uint8_t nameBuf[9];
            readBlock(STORAGE_ID, 0x8344 + b * 9, nameBuf, 9);
            std::string name = g3Decode(nameBuf, 8);
            if (name.empty()) name = "盒子 " + std::to_string(b + 1);
            boxNames.push_back(name);
        }
        // Current box: a single byte at Storage logical offset 0, clamped defensively.
        uint8_t cb = 0;
        readBlock(STORAGE_ID, 0, &cb, 1);
        if (cb < FRLG_BOX_COUNT) currentBox = cb;
    }

    void Trainer3FRLG::parseItems() {
        items.clear();
        items.resize(POUCH_COUNT3_FRLG);
        const uint16_t key16 = static_cast<uint16_t>(m_key & 0xFFFF);
        for (size_t p = 0; p < POUCH_COUNT3_FRLG; ++p) {
            const PouchInfo3FRLG& pi = getPouchInfo3FRLG(static_cast<PouchType3FRLG>(p));
            for (int i = 0; i < pi.maxSlots; ++i) {
                uint8_t entry[4];
                readBlock(LARGE_ID, pi.offset + i * 4, entry, 4);
                const uint16_t id = readUInt16LittleEndian(entry);
                uint16_t count = readUInt16LittleEndian(entry + 2);
                if (pi.keyed) count = static_cast<uint16_t>(count ^ key16);
                if (id != 0 && count != 0)
                    items[p].push_back(InventoryItem{ id, count, false, false });
            }
        }
    }

    // ------------------------------------------------------------------
    // Write path
    // ------------------------------------------------------------------
    void Trainer3FRLG::updatePartyBlock() {
        for (size_t i = 0; i < MAX_PARTY_SLOTS; ++i) {
            const size_t logical = 0x038 + i * 100;
            if (i < party.size() && party[i] && party[i]->speciesID() != 0) {
                uint8_t rec[100];
                buildPartyRecord(*party[i], rec);
                writeBlock(LARGE_ID, logical, rec, 100);
            } else {
                uint8_t blank[100] = {0};   // Gen 3 empty party slot = all zero (species 0); seed-0 crypt keeps it clean
                writeBlock(LARGE_ID, logical, blank, 100);
            }
        }
        const uint8_t count = static_cast<uint8_t>(std::min<size_t>(party.size(), MAX_PARTY_SLOTS));
        writeBlock(LARGE_ID, 0x034, &count, 1);
    }

    void Trainer3FRLG::updateBoxBlock() {
        for (size_t b = 0; b < FRLG_BOX_COUNT; ++b) {
            for (size_t s = 0; s < FRLG_BOX_SLOTS; ++s) {
                const size_t idx = b * FRLG_BOX_SLOTS + s;
                const size_t logical = 0x0004 + idx * 80;
                if (boxes[b][s] && boxes[b][s]->speciesID() != 0) {
                    uint8_t rec[80];
                    buildBoxRecord(*boxes[b][s], rec);
                    writeBlock(STORAGE_ID, logical, rec, 80);
                } else {
                    uint8_t blank[80] = {0};   // Gen 3 empty box slot = all zero
                    writeBlock(STORAGE_ID, logical, blank, 80);
                }
            }
        }
    }

    void Trainer3FRLG::updateTrainerInfoBlock() {
        // Raw sector-mapped save: OT name (g3-encoded, 7 chars @ 0x00) lives in the Small sector; money
        // in the Large block at 0x290, XOR-keyed with the security key. finalizeChecksums() runs after.
        // g3Encode writes 7 chars + a 0xFF terminator into 8 bytes, stopping before the gender byte at
        // 0x08; the UI has already rejected any name the Gen-3 glyph table can't store.
        const size_t sm = m_sectorOfs[SMALL_ID];
        if (sm + 0x09 <= saveData.size())
            g3Encode(this->trainerName, &saveData[sm + 0x00], 8, 7);
        uint8_t moneyBuf[4];
        writeUInt32LittleEndian(moneyBuf, this->money ^ m_key);
        writeBlock(LARGE_ID, 0x290, moneyBuf, 4);
    }

    void Trainer3FRLG::updateItemBlock() {
        const uint16_t key16 = static_cast<uint16_t>(m_key & 0xFFFF);
        for (size_t p = 0; p < items.size() && p < POUCH_COUNT3_FRLG; ++p) {
            const PouchInfo3FRLG& pi = getPouchInfo3FRLG(static_cast<PouchType3FRLG>(p));
            int slot = 0;
            for (const auto& it : items[p]) {
                if (slot >= pi.maxSlots) break;
                uint8_t entry[4];
                writeUInt16LittleEndian(entry, it.itemId);
                const uint16_t stored = pi.keyed ? static_cast<uint16_t>(it.count ^ key16) : it.count;
                writeUInt16LittleEndian(entry + 2, stored);
                writeBlock(LARGE_ID, pi.offset + slot * 4, entry, 4);
                ++slot;
            }
            // Zero-fill the rest: empty keyed slots store the key (so the decoded count is 0), matching the game.
            for (; slot < pi.maxSlots; ++slot) {
                uint8_t entry[4];
                writeUInt16LittleEndian(entry, 0);
                writeUInt16LittleEndian(entry + 2, pi.keyed ? key16 : 0);
                writeBlock(LARGE_ID, pi.offset + slot * 4, entry, 4);
            }
        }
        // Money is written by updateTrainerInfoBlock(), which the save flow calls alongside this;
        // keeping it there avoids a double-write of the same XOR-keyed value.
    }

    std::unique_ptr<::Pokemon::Pokemon> Trainer3FRLG::createBlankPokemon() const {
        // A zeroed PK3 is a valid empty entity in Gen 3: PID 0 => seed 0 (identity XOR) + shuffle 0
        // (identity), so decrypt leaves zeros, species 0, checksum 0 (valid). Party-sized so the creator
        // can place it into either party or a box.
        std::vector<std::byte> zero(100, std::byte{0});
        return std::make_unique<Pokemon3FRLG>(std::span<const std::byte>(zero.data(), zero.size()));
    }

    void Trainer3FRLG::finalizeChecksums() {
        // PKHeX FRLG per-sector checksum lengths (id 0 / 4 / 13 are short; the rest full 0xF80).
        static const size_t CHUNK[FRLG_SECTORS] = {
            0xF2C, 0xF80, 0xF80, 0xF80, 0xF08, 0xF80, 0xF80,
            0xF80, 0xF80, 0xF80, 0xF80, 0xF80, 0xF80, 0x7D0
        };
        for (int id = 0; id < static_cast<int>(FRLG_SECTORS); ++id) {
            const size_t off = m_sectorOfs[id];
            uint32_t sum = 0;
            for (size_t o = 0; o + 4 <= CHUNK[id]; o += 4)
                sum += readUInt32LittleEndian(&saveData[off + o]);
            const uint16_t chk = static_cast<uint16_t>((sum + (sum >> 16)) & 0xFFFF);
            writeUInt16LittleEndian(&saveData[off + 0xFF6], chk);
        }
    }
}
