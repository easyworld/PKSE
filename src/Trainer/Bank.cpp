#include "Trainer/Bank.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "Pokemon/Pokemon7LGPE.h"
#include "Pokemon/Pokemon8SWSH.h"
#include "Pokemon/Pokemon9LZA.h"
#include "Pokemon/Pokemon9SV.h"
#include "Pokemon/Pokemon8LA.h"
#include "Pokemon/Pokemon8BDSP.h"
#include "Pokemon/Pokemon3FRLG.h"
#include "Encryption/Encryption7LGPE.h"
#include "Encryption/Encryption8SWSH.h"
#include "Encryption/Encryption9LZA.h"
#include "Encryption/Encryption9SV.h"
#include "Encryption/Encryption8LA.h"
#include "Encryption/Encryption8BDSP.h"
#include "Encryption/Encryption3FRLG.h"
#include "Utils/FileUtilities.h"
#include "Utils/HelperUtilities.h"
#include "Utils/Logger.h"
#include "Globals.h"

using namespace Utils;
using namespace Enums;
using namespace Pokemon;
using namespace Encryption;

namespace Trainer {
    namespace {
        // ---- Unified on-disk format ----------------------------------------------------------
        // File: BASE_SAVE_DIRECTORY/bank/bank.dat
        //   Header (16 B): char magic[8]="PKSEBANK"; u32 version; u32 boxCount
        //   Then boxCount*BANK_SLOTS_PER_BOX fixed records, each:
        //     u32 groupTag; u8 payload[maxPayload]   (payload = native ENCRYPTED per-gen bytes)
        // An empty slot has groupTag == BTAG_EMPTY (0). Records are fixed-size (payload sized to
        // the largest party record across all games) so slot N is always at a computable offset.
        constexpr char     BANK_MAGIC[8] = { 'P','K','S','E','B','A','N','K' };
        constexpr uint32_t BANK_VERSION  = 1;
        constexpr size_t   HEADER_SIZE   = 16;  // magic[8] + version(4) + boxCount(4)

        // Optional per-box names live in a section appended AFTER the fixed records, introduced by
        // this marker. Old readers stop at the last record and ignore the trailing bytes, so adding
        // it needs no version bump -- a v1 file without the section simply has no custom names, and a
        // v1 file with it is still read correctly by code that predates the section. Per box:
        // u16 LE length + that many UTF-8 bytes.
        constexpr uint8_t  NAMES_MARKER[4] = { 'N','A','M','S' };

        // Frozen on-disk group tags -- written into every slot record. NEVER renumber these;
        // add-only. (Deliberately independent of the Enums::GameVersion numeric values, which are
        // title-ID-derived and must be free to change without invalidating banked mons.)
        enum : uint32_t {
            BTAG_EMPTY = 0,
            BTAG_GG    = 1,
            BTAG_SWSH  = 2,
            BTAG_BDSP  = 3,
            BTAG_PLA   = 4,
            BTAG_SV    = 5,
            BTAG_ZA    = 6,
            BTAG_FRLG  = 7,
        };

        uint32_t bankTagFor(GameVersion group) {
            switch (group) {
                case GameVersion::GG:   return BTAG_GG;
                case GameVersion::SWSH: return BTAG_SWSH;
                case GameVersion::BDSP: return BTAG_BDSP;
                case GameVersion::PLA:  return BTAG_PLA;
                case GameVersion::SV:   return BTAG_SV;
                case GameVersion::ZA:   return BTAG_ZA;
                case GameVersion::FRLG: return BTAG_FRLG;
                default:                return BTAG_EMPTY;
            }
        }

        // Maps a frozen tag back to its group. Returns false for BTAG_EMPTY / unknown tags.
        bool groupForBankTag(uint32_t tag, GameVersion& out) {
            switch (tag) {
                case BTAG_GG:   out = GameVersion::GG;   return true;
                case BTAG_SWSH: out = GameVersion::SWSH; return true;
                case BTAG_BDSP: out = GameVersion::BDSP; return true;
                case BTAG_PLA:  out = GameVersion::PLA;  return true;
                case BTAG_SV:   out = GameVersion::SV;   return true;
                case BTAG_ZA:   out = GameVersion::ZA;   return true;
                case BTAG_FRLG: out = GameVersion::FRLG; return true;
                default:        return false;
            }
        }

        // Per-group serialization: record size, construction, and encryption.
        size_t recordSizeFor(GameVersion group) {
            switch (group) {
                case GameVersion::GG:   return 260;                 // SIZE_PARTY7_LGPE (0x104)
                case GameVersion::SWSH: return SIZE_PARTY8_SWSH;
                case GameVersion::ZA:   return SIZE_PARTY9_LZA;
                case GameVersion::SV:   return SIZE_PARTY9_SV;
                case GameVersion::PLA:  return SIZE_PARTY8_LA;
                case GameVersion::BDSP: return SIZE_PARTY8_BDSP;
                case GameVersion::FRLG: return SIZE_PARTY3_FRLG;    // 100 (< max payload; on-disk stride unchanged)
                default:                return 260;
            }
        }

        // Largest party record across all supported games -> the fixed per-slot payload size.
        // EVERY group must appear here. FRLG was missing; it is smaller than the maximum so the
        // value didn't change, but a future format added the same way and left off this list would
        // have a record longer than the payload it is written into -- a silent overflow.
        size_t maxPayloadSize() {
            return std::max({ recordSizeFor(GameVersion::GG),   recordSizeFor(GameVersion::SWSH),
                              recordSizeFor(GameVersion::BDSP), recordSizeFor(GameVersion::PLA),
                              recordSizeFor(GameVersion::SV),   recordSizeFor(GameVersion::ZA),
                              recordSizeFor(GameVersion::FRLG) });
        }

        std::unique_ptr<::Pokemon::Pokemon> makePokemon(GameVersion group, std::span<const std::byte> rec) {
            switch (group) {
                case GameVersion::GG:   return std::make_unique<Pokemon7LGPE>(rec);
                case GameVersion::SWSH: return std::make_unique<Pokemon8SWSH>(rec);
                case GameVersion::ZA:   return std::make_unique<Pokemon9LZA>(rec);
                case GameVersion::SV:   return std::make_unique<Pokemon9SV>(rec);
                case GameVersion::PLA:  return std::make_unique<Pokemon8LA>(rec);
                case GameVersion::BDSP: return std::make_unique<Pokemon8BDSP>(rec);
                case GameVersion::FRLG: return std::make_unique<Pokemon3FRLG>(rec);
                default:                return nullptr;
            }
        }

        std::byte* encryptFor(GameVersion group, std::span<const std::byte> dec, uint32_t ec) {
            switch (group) {
                case GameVersion::GG:   return encryptArray7LGPE(dec, ec);
                case GameVersion::SWSH: return encryptArray8SWSH(dec, ec);
                case GameVersion::ZA:   return encryptArray9LZA(dec, ec);
                case GameVersion::SV:   return encryptArray9SV(dec, ec);
                case GameVersion::PLA:  return encryptArray8LA(dec, ec);
                case GameVersion::BDSP: return encryptArray8BDSP(dec, ec);
                case GameVersion::FRLG: return encryptArray3FRLG(dec);   // Gen 3 keys off the PID in-buffer (no ec)
                default:                return nullptr;
            }
        }

        void putU32LE(uint8_t* p, uint32_t v) {
            p[0] = static_cast<uint8_t>(v);
            p[1] = static_cast<uint8_t>(v >> 8);
            p[2] = static_cast<uint8_t>(v >> 16);
            p[3] = static_cast<uint8_t>(v >> 24);
        }
    }

    Bank::Bank() {
        boxes.resize(BANK_BOX_COUNT);
        load();
    }

    std::string Bank::filePath() const {
        return BASE_SAVE_DIRECTORY + "/bank/bank.dat";
    }

    std::string Bank::boxDisplayName(size_t box) const {
        if (box < BANK_BOX_COUNT && !boxNames[box].empty()) return boxNames[box];
        return "银行箱 " + std::to_string(box + 1);   // default, 1-indexed
    }

    std::vector<uint8_t> Bank::serialize() const {
        const size_t payload = maxPayloadSize();
        const size_t recSize = 4 + payload;
        const size_t total   = BANK_BOX_COUNT * BANK_SLOTS_PER_BOX;
        std::vector<uint8_t> buf(HEADER_SIZE + total * recSize, 0);

        // Header.
        std::memcpy(&buf[0], BANK_MAGIC, 8);
        putU32LE(&buf[8],  BANK_VERSION);
        putU32LE(&buf[12], static_cast<uint32_t>(BANK_BOX_COUNT));

        for (size_t box = 0; box < BANK_BOX_COUNT; ++box) {
            for (size_t slot = 0; slot < BANK_SLOTS_PER_BOX; ++slot) {
                const auto& pk = boxes[box][slot];
                if (!pk || pk->speciesID() == 0) continue;

                const GameVersion g   = pk->getGameGroup();
                const uint32_t    tag = bankTagFor(g);
                if (tag == BTAG_EMPTY) continue;  // unknown group -> can't tag it, skip

                const size_t recOff = HEADER_SIZE + (box * BANK_SLOTS_PER_BOX + slot) * recSize;
                putU32LE(&buf[recOff], tag);

                // Mirror the mon's native bytes: encrypt with its own group + EC, copy up to its
                // record size (box mons are shorter than party; the tail stays zero-padded).
                const uint32_t ec = readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(pk->getData().data()));
                std::span<const std::byte> dec(pk->getData().data(), pk->getDataSize());
                std::byte* enc = encryptFor(g, dec, ec);
                if (enc) {
                    const size_t n = std::min({ pk->getDataSize(), recordSizeFor(g), payload });
                    std::memcpy(&buf[recOff + 4], enc, n);
                    delete[] enc;
                }
            }
        }

        // Names section (see NAMES_MARKER). Appended, length-prefixed per box, so it stays
        // backward compatible with readers that only know the fixed record table.
        buf.insert(buf.end(), NAMES_MARKER, NAMES_MARKER + 4);
        for (size_t box = 0; box < BANK_BOX_COUNT; ++box) {
            std::string nm = boxNames[box];
            if (nm.size() > 0xFFFF) nm.resize(0xFFFF);   // length field is u16
            const uint16_t len = static_cast<uint16_t>(nm.size());
            buf.push_back(static_cast<uint8_t>(len & 0xFF));
            buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
            buf.insert(buf.end(), nm.begin(), nm.end());
        }
        return buf;
    }

    void Bank::load() {
        // Reset every slot first so this is a true reload -- also the "discard changes" path.
        for (auto& box : boxes)
            for (auto& slot : box)
                slot.reset();
        for (auto& n : boxNames) n.clear();   // names revert on reload/discard too

        const std::string path = filePath();
        size_t fileSize = 0;
        uint8_t* file = readAllBytes(path.c_str(), &fileSize);
        if (!file) {
            // No unified bank yet: one-time import of any legacy per-group bank files, then snapshot.
            migrateLegacyBanks();
            savedImage = serialize();
            logInfoToFile("Bank: no unified bank file; migrated legacy banks if present", path.c_str());
            return;
        }

        const size_t payload = maxPayloadSize();
        const size_t recSize = 4 + payload;

        // A file we cannot parse must be moved aside, NOT left in place to be silently overwritten
        // by the next save. Whatever it is -- a newer format, a bad card, a half-finished write --
        // it is the user's only copy of those Pokemon, and starting empty over the top of it would
        // destroy them. Renaming lets us proceed with an empty bank while the original survives.
        auto abandonFile = [&](const char* why) {
            const std::string aside = path + ".unreadable";
            std::remove(aside.c_str());                     // keep only the most recent casualty
            if (std::rename(path.c_str(), aside.c_str()) == 0)
                logErrorToFile("Bank: unreadable file preserved as bank.dat.unreadable", why);
            else
                logErrorToFile("Bank: unreadable file could NOT be preserved", why);
            delete[] file;
            savedImage = serialize();
        };

        if (fileSize < HEADER_SIZE || std::memcmp(file, BANK_MAGIC, 8) != 0) {
            abandonFile("bad magic or truncated header");
            return;
        }
        const uint32_t version = readUInt32LittleEndian(file + 8);
        if (version != BANK_VERSION) {
            // Refuse rather than misread. Parsing a future layout as v1 would manufacture garbage
            // Pokemon out of correctly-written bytes -- worse than reading nothing.
            abandonFile("unsupported bank version");
            return;
        }

        // How many boxes the FILE was written with. This is what sizes its record table, and therefore
        // where its names section begins -- using our own BANK_BOX_COUNT instead would look past the end
        // of any smaller, older bank and silently drop every custom box name the first time the count
        // was raised. Records are position-indexed, so a shorter table simply fills the low boxes.
        uint32_t fileBoxes = readUInt32LittleEndian(file + 12);
        if (fileBoxes == 0 || fileBoxes > 4096) {
            // Header damaged or absurd; fall back to the record count the file can actually hold rather
            // than trusting it to compute an offset.
            fileBoxes = static_cast<uint32_t>(BANK_BOX_COUNT);
        }
        if (fileBoxes > BANK_BOX_COUNT) {
            // The bank was written by a build with MORE boxes. Everything past ours cannot be loaded,
            // and saving would drop it, so say so loudly rather than quietly truncating someone's bank.
            logErrorToFile("Bank: file has more boxes than this build supports; extra boxes will be lost if saved",
                           std::to_string(fileBoxes).c_str());
        }
        const size_t fileTotal = static_cast<size_t>(fileBoxes) * BANK_SLOTS_PER_BOX;
        const size_t total = BANK_BOX_COUNT * BANK_SLOTS_PER_BOX;
        // Whole records only: a file truncated mid-record contributes nothing past the last
        // complete one, which is what keeps the span below inside the buffer.
        const size_t avail = (fileSize > HEADER_SIZE) ? (fileSize - HEADER_SIZE) / recSize : 0;
        const size_t count = std::min({ total, fileTotal, avail });
        loadRejects = 0;
        for (size_t n = 0; n < count; ++n) {
            const size_t recOff = HEADER_SIZE + n * recSize;
            const uint32_t tag = readUInt32LittleEndian(file + recOff);
            GameVersion g;
            if (tag == BTAG_EMPTY) continue;                       // genuinely empty slot
            if (!groupForBankTag(tag, g)) { ++loadRejects; continue; }   // unknown tag = damage

            std::span<const std::byte> rec(reinterpret_cast<const std::byte*>(file + recOff + 4),
                                           recordSizeFor(g));
            auto pk = makePokemon(g, rec);

            // Bytes off the SD card are NOT trusted. A slot that decodes to nonsense must be
            // dropped, not stored: a non-null slot holding garbage is a "ghost", and it
            // re-encrypts into a Bad Egg the moment it is withdrawn into a real save. The
            // checksum is the decisive test -- every format carries one.
            if (!pk || pk->speciesID() == 0 || pk->checksum() != pk->calculateChecksum()) {
                ++loadRejects;
                continue;
            }
            boxes[n / BANK_SLOTS_PER_BOX][n % BANK_SLOTS_PER_BOX] = std::move(pk);
        }

        // Optional names section, appended after the full record table (see NAMES_MARKER). Absent in
        // files written before names existed -- those just keep the default "Bank N" labels.
        // Positioned by the FILE's record table, not ours -- see fileBoxes above.
        const size_t namesOff = HEADER_SIZE + fileTotal * recSize;
        if (fileSize >= namesOff + 4 && std::memcmp(file + namesOff, NAMES_MARKER, 4) == 0) {
            size_t p = namesOff + 4;
            const size_t nameCount = std::min<size_t>(fileBoxes, BANK_BOX_COUNT);
            for (size_t box = 0; box < nameCount; ++box) {
                if (p + 2 > fileSize) break;                            // truncated: stop cleanly
                const uint16_t len = static_cast<uint16_t>(file[p] | (file[p + 1] << 8));
                p += 2;
                if (p + len > fileSize) break;                         // truncated string: stop
                const size_t take = std::min<size_t>(len, MAX_BOX_NAME_LEN * 4);  // UTF-8 guard
                boxNames[box].assign(reinterpret_cast<const char*>(file + p), take);
                p += len;
            }
        }

        delete[] file;
        savedImage = serialize();  // baseline snapshot of the just-loaded state
        if (loadRejects > 0)
            logErrorToFile("Bank: dropped damaged slot records on load",
                           std::to_string(loadRejects).c_str());
        logInfoToFile("Bank: loaded unified bank", path.c_str());
    }

    void Bank::migrateLegacyBanks() {
        // Legacy layout (pre-unified): one file per group, "<tag>_bank.dat", uniform
        // recordSizeFor(group) stride, no header, a slot occupied iff its EC (first u32) != 0.
        // Import every non-empty mon into the unified bank, first-fit; overflow is dropped + logged.
        struct Legacy { GameVersion group; const char* tag; };
        static const Legacy legacy[] = {
            { GameVersion::GG,   "gg"   }, { GameVersion::SWSH, "swsh" },
            { GameVersion::ZA,   "za"   }, { GameVersion::SV,   "sv"   },
            { GameVersion::PLA,  "pla"  }, { GameVersion::BDSP, "bdsp" },
        };

        size_t dstBox = 0, dstSlot = 0;
        auto placeNext = [&](std::unique_ptr<::Pokemon::Pokemon> pk) -> bool {
            while (dstBox < BANK_BOX_COUNT) {
                if (dstSlot >= BANK_SLOTS_PER_BOX) { dstSlot = 0; ++dstBox; continue; }
                if (!boxes[dstBox][dstSlot]) { boxes[dstBox][dstSlot] = std::move(pk); ++dstSlot; return true; }
                ++dstSlot;
            }
            return false;  // bank full
        };

        int imported = 0, dropped = 0;
        for (const auto& L : legacy) {
            const std::string p = BASE_SAVE_DIRECTORY + "/bank/" + L.tag + "_bank.dat";
            size_t sz = 0;
            uint8_t* f = readAllBytes(p.c_str(), &sz);
            if (!f) continue;

            const size_t recSize = recordSizeFor(L.group);
            const size_t n = recSize ? (sz / recSize) : 0;
            for (size_t i = 0; i < n; ++i) {
                const size_t off = i * recSize;
                if (readUInt32LittleEndian(f + off) == 0) continue;  // legacy empty (EC == 0)
                std::span<const std::byte> rec(reinterpret_cast<const std::byte*>(f + off), recSize);
                auto pk = makePokemon(L.group, rec);
                if (pk && pk->speciesID() != 0) {
                    if (placeNext(std::move(pk))) ++imported; else ++dropped;
                }
            }
            delete[] f;
        }

        if (imported > 0 || dropped > 0) {
            const std::string msg = std::to_string(imported) + " imported, " + std::to_string(dropped) + " dropped";
            logInfoToFile("Bank: migrated legacy per-group banks", msg.c_str());
        }
    }

    size_t Bank::verifyImage(const std::vector<uint8_t>& image) const {
        const size_t payload = maxPayloadSize();
        const size_t recSize = 4 + payload;
        size_t failures = 0;

        for (size_t box = 0; box < BANK_BOX_COUNT; ++box) {
            for (size_t slot = 0; slot < BANK_SLOTS_PER_BOX; ++slot) {
                const size_t recOff = HEADER_SIZE + (box * BANK_SLOTS_PER_BOX + slot) * recSize;
                if (recOff + recSize > image.size()) { ++failures; continue; }

                const auto& src = boxes[box][slot];
                const uint32_t tag = readUInt32LittleEndian(image.data() + recOff);
                const bool occupied = src && src->speciesID() != 0 &&
                                      bankTagFor(src->getGameGroup()) != BTAG_EMPTY;
                if (!occupied) {
                    if (tag != BTAG_EMPTY) ++failures;   // wrote a record for a slot we consider empty
                    continue;
                }

                GameVersion g;
                if (!groupForBankTag(tag, g) || g != src->getGameGroup()) { ++failures; continue; }

                auto rt = makePokemon(g, std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(image.data() + recOff + 4), recordSizeFor(g)));
                if (!rt) { ++failures; continue; }

                // Compare exactly the span serialize() wrote -- a box mon is shorter than the party
                // record it sits in, and the zero tail beyond it is padding, not data. Comparing
                // past that would fail on padding rather than on any real difference.
                const size_t n = std::min({ src->getDataSize(), rt->getDataSize(), recordSizeFor(g) });
                if (std::memcmp(src->getData().data(), rt->getData().data(), n) != 0) {
                    ++failures;
                    const std::string where = "box " + std::to_string(box + 1) +
                                              " slot " + std::to_string(slot + 1) +
                                              " (" + std::string(src->species()) + ")";
                    logErrorToFile("Bank: slot failed encrypt/decrypt round trip", where.c_str());
                }
            }
        }
        return failures;
    }

    bool Bank::save() const {
        // Ensure the bank directory exists.
        const std::string dir = BASE_SAVE_DIRECTORY + "/bank";
        mkdir(dir.c_str(), 0777);  // ignore EEXIST

        std::vector<uint8_t> buf = serialize();

        // The bank's contract is byte-in == byte-out, and nothing used to check it. Verify the
        // image reproduces every live Pokemon before it goes to disk.
        //
        // Deliberately does NOT abort the save. A failed bank save blocks leaving the storage view
        // so a false positive here would trap the user in the UI; and if the mismatch is
        // real, refusing to write leaves them with a stale file rather than a fresh one. Record it,
        // log which slot, and let the caller surface it.
        verifyFailures = verifyImage(buf);

        const std::string path = filePath();
        FILE* f = fopen(path.c_str(), "wb");
        if (!f) {
            logErrorToFile("Bank: failed to open bank file for writing", path.c_str());
            return false;
        }
        const size_t written = fwrite(buf.data(), 1, buf.size(), f);
        fclose(f);
        if (written != buf.size()) return false;

        savedImage = std::move(buf);  // in sync with disk again
        return true;
    }

    bool Bank::hasChanged() const {
        return serialize() != savedImage;
    }
}
