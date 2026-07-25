#include "Conversion/Convert.h"

#include <cstdint>
#include <span>
#include <vector>
#include <bit>
#include <ctime>     // std::time / std::localtime -> HOME-style transfer date for Gen 3 (no met date)

#include "Pokemon/Pokemon8SWSH.h"
#include "Pokemon/Pokemon8BDSP.h"
#include "Pokemon/Pokemon8LA.h"
#include "Pokemon/Pokemon7LGPE.h"
#include "Pokemon/Pokemon9SV.h"
#include "Pokemon/Pokemon9LZA.h"
#include "Pokemon/Pokemon3FRLG.h"        // PK3 entity + g3ToNational / nationalToG3
#include "Pokemon/BaseStatsGen89.h"      // getSpeciesNameGen89 (Gen 3 nickname = uppercase species name)
#include "Pokemon/PersonalInfoTable.h"
#include "Pokemon/PokemonTypes.h"       // getPokemonTypes -> Tera type for cross-gen PK8->PK9
#include "Pokemon/LearnsetTable.h"      // isLearnable -> sanitize the moveset to the destination game
#include "Pokemon/Experience.h"        // getLevelFromExp / getGrowthRate -> seed a fresh party-stat tail
#include "Encryption/Encryption8SWSH.h"
#include "Encryption/Encryption8BDSP.h"
#include "Encryption/Encryption8LA.h"
#include "Encryption/Encryption7LGPE.h"
#include "Encryption/Encryption9SV.h"
#include "Encryption/Encryption9LZA.h"
#include "Encryption/Encryption3FRLG.h"
#include "Names/ItemNames.h"         // itemG3ToModern / itemModernToG3 (Gen 3 <-> modern held item)
#include "Names/ItemPresence.h"      // isHeldItemPresent -> sanitize the held item to the destination
#include "Utils/HelperUtilities.h"   // readUInt32LittleEndian

using Enums::GameVersion;

namespace Conversion {
    namespace {
        // Rebuild the destination entity from an ENCRYPTED record (the subclass ctor decrypts).
        std::unique_ptr<Pokemon::Pokemon> makePokemon(GameVersion g, std::span<const std::byte> rec) {
            switch (g) {
                case GameVersion::GG:   return std::make_unique<Pokemon::Pokemon7LGPE>(rec);
                case GameVersion::SWSH: return std::make_unique<Pokemon::Pokemon8SWSH>(rec);
                case GameVersion::BDSP: return std::make_unique<Pokemon::Pokemon8BDSP>(rec);
                case GameVersion::PLA:  return std::make_unique<Pokemon::Pokemon8LA>(rec);
                case GameVersion::SV:   return std::make_unique<Pokemon::Pokemon9SV>(rec);
                case GameVersion::ZA:   return std::make_unique<Pokemon::Pokemon9LZA>(rec);
                case GameVersion::FRLG: return std::make_unique<Pokemon::Pokemon3FRLG>(rec);
                default:                return nullptr;
            }
        }

        // Encrypt a decrypted buffer with the destination format's crypto (all four use Encrypt8).
        std::byte* encryptFor(GameVersion g, std::span<const std::byte> dec, uint32_t ec) {
            switch (g) {
                case GameVersion::GG:   return Encryption::encryptArray7LGPE(dec, ec);
                case GameVersion::SWSH: return Encryption::encryptArray8SWSH(dec, ec);
                case GameVersion::BDSP: return Encryption::encryptArray8BDSP(dec, ec);
                case GameVersion::PLA:  return Encryption::encryptArray8LA(dec, ec);
                case GameVersion::SV:   return Encryption::encryptArray9SV(dec, ec);
                case GameVersion::ZA:   return Encryption::encryptArray9LZA(dec, ec);
                case GameVersion::FRLG: return Encryption::encryptArray3FRLG(dec);   // Gen 3 keys off the in-buffer PID
                default:                return nullptr;
            }
        }

        uint8_t presenceBit(GameVersion g) {
            switch (g) {
                case GameVersion::GG:   return Pokemon::PERSONAL_GAME_GG;
                case GameVersion::SWSH: return Pokemon::PERSONAL_GAME_SWSH;
                case GameVersion::BDSP: return Pokemon::PERSONAL_GAME_BDSP;
                case GameVersion::PLA:  return Pokemon::PERSONAL_GAME_PLA;
                case GameVersion::SV:   return Pokemon::PERSONAL_GAME_SV;
                case GameVersion::ZA:   return Pokemon::PERSONAL_GAME_ZA;
                default:                return 0;
            }
        }

        // PK8-layout group (SwSh/BDSP) vs PK9-layout group (S/V/Z-A). LGPE (GG) and Legends: Arceus
        // (PLA) use different containers/crypto and are handled by neither -> Unsupported.
        inline bool isG8(GameVersion g) { return g == GameVersion::SWSH || g == GameVersion::BDSP; }
        inline bool isG9(GameVersion g) { return g == GameVersion::SV   || g == GameVersion::ZA;   }

        // PK8/PK9 party record: the 0x148 stored block + a 0x10-byte battle-stat tail (level @0x148,
        // HP/ATK/DEF/SPE/SPA/SPD @0x14A-0x155). The tail is NOT part of the checksummed/encrypted region,
        // but it IS real storage the entity's level()/statXXX() accessors index -- so every buffer handed
        // to a Gen 8/9 entity must be this long, not the stored size.
        constexpr size_t PARTY_SIZE_G89 = 0x158;

        // Whether a supported+allowed conversion from src -> dest exists (no allocation).
        Result gate(const Pokemon::Pokemon& src, GameVersion dest) {
            const GameVersion from = src.getGameGroup();
            if (from == dest) return Result::SameGroup;

            // Phase B supports every mainline pairing. All conversions normalize through the PK8 layout:
            // same-gen siblings (SwSh<->BDSP, S/V<->Z-A) share it; PK9 reaches it via transformG9toG8;
            // Legends: Arceus (PA8) and Let's Go (PB7) reach it via their remaps. Full two-way for all six
            // games -- deliberately beyond HOME's one-way Let's Go limit; the dex-presence check below is
            // the only species gate (LGPE = Kanto + Meltan/Melmetal + Alolan forms).
            auto inFamily = [](GameVersion g) {
                return isG8(g) || isG9(g) || g == GameVersion::PLA || g == GameVersion::GG
                    || g == GameVersion::FRLG;
            };
            if (!inFamily(from) || !inFamily(dest)) return Result::Unsupported;

            const uint16_t species = src.speciesID();
            const uint8_t  form    = src.form();
            const Pokemon::PersonalInfo& pi = Pokemon::getPersonalInfo(species, form);
            // A form >= the species' form count can't exist in ANY destination. Guard it explicitly:
            // getPersonalInfo() clamps an out-of-range form back to form 0, so without this a corrupt
            // form would read form-0's presence and silently false-pass the dex checks below (then get
            // written verbatim into the destination as an out-of-range form -> bad egg in-game).
            if (form != 0 && form >= Pokemon::getPersonalInfo(species, 0).formCount)
                return Result::NotInDex;
            // Dex-presence gate. FRLG has no personal-table presence bit, so use the Gen 3 species range
            // (National <= 386 that maps to a valid internal id). Every other game uses its presence bit.
            if (dest == GameVersion::FRLG) {
                if (Pokemon::nationalToG3(species) == 0) return Result::NotInDex;   // not obtainable in Gen 3
                // Gen 3 stores no general form byte, so an alternate form (Alolan/Galarian/Hisuian/Paldean
                // variant, a Paldean Tauros breed, an alternate Deoxys, ...) would be silently FLATTENED to
                // the base form on write -- a Combat Breed Tauros arriving in FireRed as a plain Tauros.
                // Refuse instead of corrupting, mirroring PKHeX/HOME. Unown is the one exception: its form
                // rides on the PID (which the remap carries), so Gen 3 re-derives the same letter.
                if (form != 0 && species != 201 /*Unown*/) return Result::NotInDex;
            } else if ((pi.presence & presenceBit(dest)) == 0) {
                return Result::NotInDex;   // species, or this form specifically, out-of-dex -> refuse
            }
            if (dest == GameVersion::BDSP && (species == 327 /*Spinda*/ || species == 290 /*Nincada*/))
                return Result::Blocked;                                            // PKHeX refuses these into BDSP
            return Result::Ok;
        }

        // ---- Cross-gen field remap (PK8 layout <-> PK9 layout) --------------------------------
        // PK8 (SwSh/BDSP) and PK9 (S/V/Z-A) share the 0x148 stored size + Add16 checksum, but a set
        // of fields live at different offsets or bit positions. We CARRY origin identity + every stat
        // field verbatim (PKSE bank philosophy) and only re-lay-out the diverging regions -- unlike
        // PKHeX we do NOT regenerate moves/ability from the destination learnset and do NOT drop the
        // held item (the legality checker flags an out-of-dex moveset instead). Offsets: PKHeX
        // G8PKM.cs / PK9.cs; full map in docs/ROADMAP_TO_V1.md App. B.
        inline uint8_t  rd8 (const std::vector<std::byte>& b, size_t o) { return static_cast<uint8_t>(b[o]); }
        inline void     wr8 (std::vector<std::byte>& b, size_t o, uint8_t v) { b[o] = static_cast<std::byte>(v); }
        inline uint64_t rd64(const std::vector<std::byte>& b, size_t o) {
            uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(rd8(b, o + i)) << (8 * i); return v;
        }
        inline void wr64(std::vector<std::byte>& b, size_t o, uint64_t v) {
            for (int i = 0; i < 8; ++i) wr8(b, o + i, static_cast<uint8_t>(v >> (8 * i)));
        }
        inline void zeroRange(std::vector<std::byte>& b, size_t o, size_t n) {
            for (size_t i = 0; i < n && (o + i) < b.size(); ++i) b[o + i] = std::byte{0};
        }
        inline void wrf32(std::vector<std::byte>& b, size_t o, float v) {
            const uint32_t bits = std::bit_cast<uint32_t>(v);
            for (int i = 0; i < 4; ++i) wr8(b, o + i, static_cast<uint8_t>(bits >> (8 * i)));
        }
        inline uint16_t rd16(const std::vector<std::byte>& b, size_t o) {
            return static_cast<uint16_t>(rd8(b, o)) | (static_cast<uint16_t>(rd8(b, o + 1)) << 8);
        }
        inline uint32_t rd32(const std::vector<std::byte>& b, size_t o) {
            return static_cast<uint32_t>(rd16(b, o)) | (static_cast<uint32_t>(rd16(b, o + 2)) << 16);
        }
        inline void wr16(std::vector<std::byte>& b, size_t o, uint16_t v) {
            wr8(b, o, static_cast<uint8_t>(v)); wr8(b, o + 1, static_cast<uint8_t>(v >> 8));
        }
        inline void wr32(std::vector<std::byte>& b, size_t o, uint32_t v) {
            wr16(b, o, static_cast<uint16_t>(v)); wr16(b, o + 2, static_cast<uint16_t>(v >> 16));
        }

        // ---- Gen 3 (GBA) <-> Unicode text for cross-gen nickname/OT (EN subset; 0xFF terminator) ----
        inline char16_t g3ToU16(uint8_t b) {
            if (b == 0x00) return u' ';
            if (b >= 0xA1 && b <= 0xAA) return static_cast<char16_t>(u'0' + (b - 0xA1));
            if (b >= 0xBB && b <= 0xD4) return static_cast<char16_t>(u'A' + (b - 0xBB));
            if (b >= 0xD5 && b <= 0xEE) return static_cast<char16_t>(u'a' + (b - 0xD5));
            if (b == 0xAB) return u'!';
            if (b == 0xAC) return u'?';
            if (b == 0xAD) return u'.';
            if (b == 0xAE) return u'-';
            return u'\0';   // unmapped -> skip
        }
        inline uint8_t u16ToG3(char16_t c) {
            if (c == u' ') return 0x00;
            if (c >= u'0' && c <= u'9') return static_cast<uint8_t>(0xA1 + (c - u'0'));
            if (c >= u'A' && c <= u'Z') return static_cast<uint8_t>(0xBB + (c - u'A'));
            if (c >= u'a' && c <= u'z') return static_cast<uint8_t>(0xD5 + (c - u'a'));
            if (c == u'!') return 0xAB;
            if (c == u'?') return 0xAC;
            if (c == u'.') return 0xAD;
            if (c == u'-') return 0xAE;
            return 0xFF;    // unmappable -> terminator
        }
        // Gen 3 name (maxG3 bytes @ s+soff) -> UTF-16LE (dstBytes @ d+doff, zero-terminated + zero-padded).
        void g3NameToUtf16(std::vector<std::byte>& d, size_t doff, const std::vector<std::byte>& s, size_t soff,
                           int maxG3, int dstBytes) {
            int di = 0;
            for (int i = 0; i < maxG3 && di + 2 <= dstBytes; ++i) {
                const uint8_t b = rd8(s, soff + i);
                if (b == 0xFF) break;
                const char16_t c = g3ToU16(b);
                if (c == u'\0') continue;
                wr16(d, doff + di, static_cast<uint16_t>(c));
                di += 2;
            }
            for (; di + 1 < dstBytes; di += 2) wr16(d, doff + di, 0);   // terminator + padding
        }
        // UTF-16LE name (srcBytes @ s+soff) -> Gen 3 (maxG3 bytes @ d+doff, 0xFF-terminated + 0xFF-padded).
        void utf16ToG3Name(std::vector<std::byte>& d, size_t doff, const std::vector<std::byte>& s, size_t soff,
                           int srcBytes, int maxG3) {
            int gi = 0;
            for (int i = 0; i + 2 <= srcBytes && gi < maxG3; i += 2) {
                const char16_t c = static_cast<char16_t>(rd16(s, soff + i));
                if (c == 0) break;
                const uint8_t b = u16ToG3(c);
                if (b == 0xFF) break;
                wr8(d, doff + gi, b);
                ++gi;
            }
            for (; gi < maxG3; ++gi) wr8(d, doff + gi, 0xFF);   // Gen 3 pads names with 0xFF
        }
        // Write an ASCII string (e.g. the uppercase species name) as a Gen 3 name (0xFF-terminated/padded).
        void asciiToG3Name(std::vector<std::byte>& d, size_t doff, const char* str, int maxG3) {
            int gi = 0;
            for (const char* p = str; *p && gi < maxG3; ++p) {
                const uint8_t b = u16ToG3(static_cast<char16_t>(static_cast<unsigned char>(*p)));
                if (b == 0xFF) continue;
                wr8(d, doff + gi, b);
                ++gi;
            }
            for (; gi < maxG3; ++gi) wr8(d, doff + gi, 0xFF);
        }

        // PK8 -> PK9, in place. `species`/`form` are the source's (for the imported Tera type).
        void transformG8toG9(std::vector<std::byte>& b, uint16_t species, uint8_t form) {
            if (b.size() < 0x148) return;
            // 0x16 bit4 CanGigantamax -> PK9 has no G-Max: clear it.
            wr8(b, 0x16, rd8(b, 0x16) & ~0x10);
            // 0x22 gender: PK8 (byte>>2)&3 -> PK9 (byte>>1)&3. Keep Fateful(bit0); drop PK8 Flag2(bit1).
            { uint8_t v = rd8(b, 0x22); uint8_t fateful = v & 0x01; uint8_t gender = (v >> 2) & 0x03;
              wr8(b, 0x22, fateful | (gender << 1)); }
            // 0x48-0x57: PK8 Sociability(0x48) + Height(0x50)/Weight(0x51) -> PK9 Height(0x48)/Weight(0x49)/
            //            Scale(0x4A) + DLC move-record flags(0x4B-0x57). PK8 has no scale -> reuse height.
            { uint8_t h = rd8(b, 0x50), w = rd8(b, 0x51); zeroRange(b, 0x48, 0x10);
              wr8(b, 0x48, h); wr8(b, 0x49, w); wr8(b, 0x4A, h); }
            // 0x90-0x9F: PK8 DynamaxLevel(0x90)/Status(0x94)/Palma(0x98) -> PK9 Status(0x90)/Tera(0x94,0x95).
            //            Zero, then import a Tera from the species' primary type (Normal falls back to type2).
            { zeroRange(b, 0x90, 0x10);
              Pokemon::TypePair tp = Pokemon::getPokemonTypes(species, form);
              uint8_t tera = (tp.type1 == 0 /*Normal*/ && tp.type2 != 255) ? tp.type2 : tp.type1;
              wr8(b, 0x94, tera); wr8(b, 0x95, tera); }
            // 0xCE-0xF7 Block C: PK8 PokeJob/Version(0xDE)/BattleVer(0xDF)/Language(0xE2)/FormArg(0xE4)/
            //            AffixedRibbon(0xE8) -> PK9 Version(0xCE)/BattleVer(0xCF)/FormArg(0xD0)/Affixed(0xD4)/
            //            Language(0xD5). Capture, wipe the whole block, rewrite at PK9 offsets.
            { uint8_t version = rd8(b, 0xDE), battleVer = rd8(b, 0xDF), language = rd8(b, 0xE2), affixed = rd8(b, 0xE8);
              uint8_t f0 = rd8(b, 0xE4), f1 = rd8(b, 0xE5), f2 = rd8(b, 0xE6), f3 = rd8(b, 0xE7);
              zeroRange(b, 0xCE, 0x2A);   // 0xCE..0xF7
              wr8(b, 0xCE, version); wr8(b, 0xCF, battleVer);
              wr8(b, 0xD0, f0); wr8(b, 0xD1, f1); wr8(b, 0xD2, f2); wr8(b, 0xD3, f3);
              wr8(b, 0xD4, affixed); wr8(b, 0xD5, language); }
            // 0x11F ObedienceLevel (PK9 adds this; PK8 leaves it padding) = MetLevel.
            wr8(b, 0x11F, rd8(b, 0x125) & 0x7F);
            // 0x127-0x147: PK8 TR flags(0x127-0x134) + HOME Tracker(0x135) -> PK9 HOME Tracker(0x127) +
            //              move-record base flags(0x12F-0x147). Carry the tracker; drop the record flags.
            { uint64_t tracker = rd64(b, 0x135); zeroRange(b, 0x127, 0x21); wr64(b, 0x127, tracker); }
            // 0x156 DynamaxType (PK8 party region) -> PK9 has none. (Bounds-guarded for stored-size buffers.)
            zeroRange(b, 0x156, 0x02);
        }

        // PK9 -> PK8, in place (the mirror of transformG8toG9). Tera / ObedienceLevel / records are dropped.
        void transformG9toG8(std::vector<std::byte>& b) {
            if (b.size() < 0x148) return;
            // 0x22 gender: PK9 (byte>>1)&3 -> PK8 (byte>>2)&3. Keep Fateful(bit0); PK8 Flag2(bit1) stays 0.
            { uint8_t v = rd8(b, 0x22); uint8_t fateful = v & 0x01; uint8_t gender = (v >> 1) & 0x03;
              wr8(b, 0x22, fateful | (gender << 2)); }
            // 0x48-0x57: PK9 Height(0x48)/Weight(0x49)/Scale(0x4A) -> PK8 Sociability(0x48) + Height(0x50)/
            //            Weight(0x51). Capture height/weight, wipe, rewrite; Scale is dropped (no PK8 field).
            { uint8_t h = rd8(b, 0x48), w = rd8(b, 0x49); zeroRange(b, 0x48, 0x10);
              wr8(b, 0x50, h); wr8(b, 0x51, w); }
            // 0x90-0x9F: PK9 Status(0x90)/Tera(0x94,0x95) -> PK8 DynamaxLevel(0x90)/Status(0x94)/Palma.
            //            Tera is dropped; leave Dynamax/Status/Palma zeroed.
            zeroRange(b, 0x90, 0x10);
            // 0xCE-0xF7 Block C: PK9 Version(0xCE)/BattleVer(0xCF)/FormArg(0xD0)/Affixed(0xD4)/Language(0xD5)
            //            -> PK8 Version(0xDE)/BattleVer(0xDF)/Language(0xE2)/FormArg(0xE4)/Affixed(0xE8).
            { uint8_t version = rd8(b, 0xCE), battleVer = rd8(b, 0xCF), affixed = rd8(b, 0xD4), language = rd8(b, 0xD5);
              uint8_t f0 = rd8(b, 0xD0), f1 = rd8(b, 0xD1), f2 = rd8(b, 0xD2), f3 = rd8(b, 0xD3);
              zeroRange(b, 0xCE, 0x2A);   // 0xCE..0xF7
              wr8(b, 0xDE, version); wr8(b, 0xDF, battleVer); wr8(b, 0xE2, language);
              wr8(b, 0xE4, f0); wr8(b, 0xE5, f1); wr8(b, 0xE6, f2); wr8(b, 0xE7, f3);
              wr8(b, 0xE8, affixed); }
            // 0x11F ObedienceLevel -> PK8 padding.
            wr8(b, 0x11F, 0);
            // 0x127-0x147: PK9 HOME Tracker(0x127) + record flags(0x12F-0x147) -> PK8 TR flags(0x127-0x134)
            //              + HOME Tracker(0x135). Carry the tracker; TR flags come out zeroed.
            { uint64_t tracker = rd64(b, 0x127); zeroRange(b, 0x127, 0x21); wr64(b, 0x135, tracker); }
        }

        // ---- Legends: Arceus (PA8) <-> PK8 remap ----------------------------------------------
        // PA8 (0x168 stored / 0x178 party) shares the header + block-A core (EC..pokerus) and the
        // ribbon/mark region at identical offsets with PK8, but its whole Block B/C/D is at different
        // offsets (moves 0x54, nickname 0x60, IV32 0x94, HT 0xB8, Version 0xEE, Language 0xF2, OT 0x110,
        // ball 0x137, ...). So this is a field RELOCATION into a fresh dest-sized buffer, not an in-place
        // fixup. PA8-only data (GVs, Alpha flag/move, move-mastery/purchase records, absolute size) is
        // dropped; PK8<->PK9 legs are handled by composing with transformG8toG9 / transformG9toG8. Offsets
        // per PKHeX PA8.cs / G8PKM.cs. Party stats are recomputed on load, so 0x148+/0x168+ is left zero.
        inline void copyBytes(std::vector<std::byte>& d, size_t doff, const std::vector<std::byte>& s, size_t soff, size_t n) {
            for (size_t i = 0; i < n && (soff + i) < s.size() && (doff + i) < d.size(); ++i) d[doff + i] = s[soff + i];
        }

        // PA8 -> PK8 layout (SwSh/BDSP), returns a 0x148 stored buffer.
        std::vector<std::byte> remapPA8toPK8(const std::vector<std::byte>& s) {
            std::vector<std::byte> d(0x148, std::byte{0});
            copyBytes(d, 0x00, s, 0x00, 0x34);   // EC..pokerus/pad (species/item/id/exp/ability/PID/nature/EVs/contest); gender byte 0x22 shares PK8 bit layout
            copyBytes(d, 0x34, s, 0x34, 0x0A);   // ribbons u32 x2 + ribbon counts (0x34..0x3D)
            copyBytes(d, 0x40, s, 0x40, 0x08);   // marks (0x40..0x47); skips PA8 AlphaMove at 0x3E-0x3F
            copyBytes(d, 0x50, s, 0x50, 0x02);   // Height + Weight scalars (shared 0x50/0x51)
            wr8(d, 0x16, rd8(d, 0x16) & ~0x60);  // clear PA8-only 0x16 bit5 (IsAlpha) / bit6 (IsNoble)
            copyBytes(d, 0x72, s, 0x54, 0x08);   // Move 1-4
            copyBytes(d, 0x7A, s, 0x5C, 0x04);   // Move PP
            copyBytes(d, 0x7E, s, 0x86, 0x04);   // Move PP-Ups
            copyBytes(d, 0x82, s, 0x8A, 0x08);   // Relearn 1-4
            copyBytes(d, 0x58, s, 0x60, 0x1A);   // Nickname (26 bytes)
            copyBytes(d, 0x8C, s, 0x94, 0x04);   // IV32
            copyBytes(d, 0xA8, s, 0xB8, 0x1A);   // HT name (26 bytes)
            copyBytes(d, 0xC2, s, 0xD2, 0x03);   // HT gender / HT language / CurrentHandler (0xD2..0xD4 -> 0xC2..0xC4)
            copyBytes(d, 0xC8, s, 0xD8, 0x06);   // HT friendship + HT memory (0xD8..0xDD -> 0xC8..0xCD)
            copyBytes(d, 0xDE, s, 0xEE, 0x02);   // Version + BattleVersion
            copyBytes(d, 0xE2, s, 0xF2, 0x01);   // Language
            copyBytes(d, 0xE4, s, 0xF4, 0x04);   // FormArgument (u32)
            copyBytes(d, 0xE8, s, 0xF8, 0x01);   // AffixedRibbon
            copyBytes(d, 0xF8, s, 0x110, 0x1A);  // OT name (26 bytes)
            copyBytes(d, 0x112, s, 0x12A, 0x07); // OT friendship + OT memory (0x12A..0x130 -> 0x112..0x118)
            copyBytes(d, 0x119, s, 0x131, 0x06); // egg date + met date (0x131..0x136 -> 0x119..0x11E)
            copyBytes(d, 0x120, s, 0x138, 0x04); // egg location + met location (u16 each)
            { uint8_t ball = rd8(s, 0x137); wr8(d, 0x124, ball > 26 ? 4 : ball); }  // LA-only balls -> Poke Ball (PKHeX GetBall)
            copyBytes(d, 0x125, s, 0x13D, 0x01); // MetLevel(bits0-6) + OTGender(bit7)
            copyBytes(d, 0x126, s, 0x13E, 0x01); // HyperTrain flags
            copyBytes(d, 0x135, s, 0x14D, 0x08); // HOME Tracker (u64)
            return d;
        }

        // PK8 layout (SwSh/BDSP) -> PA8, returns a 0x178 party buffer. Mirror of remapPA8toPK8; the
        // PA8-only fields (GVs, Alpha/Noble, AlphaMove, move-mastery, absolute height/weight) stay zero,
        // so the result is a well-formed non-Alpha Hisui entity carrying the source's identity + moves.
        std::vector<std::byte> remapPK8toPA8(const std::vector<std::byte>& s) {
            std::vector<std::byte> d(0x178, std::byte{0});
            copyBytes(d, 0x00, s, 0x00, 0x34);   // EC..pokerus/pad
            copyBytes(d, 0x34, s, 0x34, 0x0A);   // ribbons + counts
            copyBytes(d, 0x40, s, 0x40, 0x08);   // marks
            copyBytes(d, 0x50, s, 0x50, 0x02);   // Height + Weight scalars
            wr8(d, 0x52, rd8(s, 0x50));          // PA8 Scale := Height (PK8/PK9 scale was dropped en route)
            wr8(d, 0x16, rd8(d, 0x16) & ~0x10);  // clear PK8-only 0x16 bit4 (CanGigantamax)
            copyBytes(d, 0x54, s, 0x72, 0x08);   // Move 1-4
            copyBytes(d, 0x5C, s, 0x7A, 0x04);   // Move PP
            copyBytes(d, 0x86, s, 0x7E, 0x04);   // Move PP-Ups
            copyBytes(d, 0x8A, s, 0x82, 0x08);   // Relearn 1-4
            copyBytes(d, 0x60, s, 0x58, 0x1A);   // Nickname
            copyBytes(d, 0x94, s, 0x8C, 0x04);   // IV32
            copyBytes(d, 0xB8, s, 0xA8, 0x1A);   // HT name
            copyBytes(d, 0xD2, s, 0xC2, 0x03);   // HT gender / language / CurrentHandler
            copyBytes(d, 0xD8, s, 0xC8, 0x06);   // HT friendship + memory
            copyBytes(d, 0xEE, s, 0xDE, 0x02);   // Version + BattleVersion
            copyBytes(d, 0xF2, s, 0xE2, 0x01);   // Language
            copyBytes(d, 0xF4, s, 0xE4, 0x04);   // FormArgument
            copyBytes(d, 0xF8, s, 0xE8, 0x01);   // AffixedRibbon
            copyBytes(d, 0x110, s, 0xF8, 0x1A);  // OT name
            copyBytes(d, 0x12A, s, 0x112, 0x07); // OT friendship + memory
            copyBytes(d, 0x131, s, 0x119, 0x06); // egg + met dates
            copyBytes(d, 0x138, s, 0x120, 0x04); // egg + met locations
            copyBytes(d, 0x137, s, 0x124, 0x01); // Ball (PK8 balls are valid in PA8 -> keep)
            copyBytes(d, 0x13D, s, 0x125, 0x01); // MetLevel + OTGender
            copyBytes(d, 0x13E, s, 0x126, 0x01); // HyperTrain flags
            copyBytes(d, 0x14D, s, 0x135, 0x08); // HOME Tracker
            return d;
        }

        // ---- Let's Go (PB7) <-> PK8 remap -----------------------------------------------------
        // PB7 is a 260-byte Gen-7-format record (Gen6/7 crypto handled by the ctor/encryptFor). Almost
        // every field is at a different offset than PK8, AND several are re-packed (PB7 ability is a u8,
        // gender+form share byte 0x1D, PB7 has no mint so PK8 StatNature := Nature). Stat-training is NOT
        // cross-converted: neither AVs (PB7 0x24-0x29) nor EVs (0x1E-0x23) are carried, so a mon entering
        // LGPE gets AVs=0 and a mon leaving gets EVs=0 (per product spec -- the UI asks the user to ack it).
        // PB7-only data (AVs, CP, Height/WeightAbsolute, Spirit/Mood) is dropped; CP + party stats are
        // recomputed on load. Offsets per PKHeX PB7.cs. Ribbons differ in encoding and are dropped.

        // PB7 -> PK8 layout (SwSh/BDSP), returns a 0x148 stored buffer.
        std::vector<std::byte> remapPB7toPK8(const std::vector<std::byte>& s) {
            std::vector<std::byte> d(0x148, std::byte{0});
            copyBytes(d, 0x00, s, 0x00, 0x08);   // EC + Sanity + Checksum (checksum recomputed later)
            copyBytes(d, 0x08, s, 0x08, 0x0C);   // Species / HeldItem / ID32 / EXP (0x08..0x13 shared)
            wr8(d, 0x14, rd8(s, 0x14));          // Ability (PB7 u8 -> PK8 u16 low byte; high byte stays 0)
            wr8(d, 0x16, rd8(s, 0x15) & 0x07);   // AbilityNumber (PB7 0x15 bits0-2 -> PK8 0x16 bits0-2)
            copyBytes(d, 0x18, s, 0x16, 0x02);   // MarkingValue (PB7 0x16 -> PK8 0x18)
            copyBytes(d, 0x1C, s, 0x18, 0x04);   // PID (PB7 0x18 -> PK8 0x1C)
            wr8(d, 0x20, rd8(s, 0x1C));          // Nature (PB7 0x1C -> PK8 0x20)
            wr8(d, 0x21, rd8(s, 0x1C));          // StatNature := Nature (LGPE has no mint)
            { uint8_t b = rd8(s, 0x1D); uint8_t fateful = b & 0x01, gender = (b >> 1) & 0x03, form = b >> 3;
              wr8(d, 0x22, fateful | (gender << 2));  // PK8 0x22: Fateful(bit0) + Gender(bits2-3)
              wr8(d, 0x24, form); }                    // PK8 0x24: Form
            // EVs (0x1E-0x23) + AVs (0x24-0x29) intentionally NOT carried -> PK8 EVs stay 0.
            wr8(d, 0x32, rd8(s, 0x2B));          // PokerusState (PB7 0x2B -> PK8 0x32)
            copyBytes(d, 0x50, s, 0x3A, 0x02);   // Height + Weight scalars (PB7 0x3A/0x3B -> PK8 0x50/0x51)
            copyBytes(d, 0xE4, s, 0x3C, 0x04);   // FormArgument (PB7 0x3C -> PK8 0xE4)
            copyBytes(d, 0x58, s, 0x40, 0x1A);   // Nickname
            copyBytes(d, 0x72, s, 0x5A, 0x08);   // Moves
            copyBytes(d, 0x7A, s, 0x62, 0x04);   // Move PP
            copyBytes(d, 0x7E, s, 0x66, 0x04);   // Move PP-Ups
            copyBytes(d, 0x82, s, 0x6A, 0x08);   // Relearn
            copyBytes(d, 0x8C, s, 0x74, 0x04);   // IV32
            copyBytes(d, 0xA8, s, 0x78, 0x1A);   // HT name
            wr8(d, 0xC2, rd8(s, 0x92));          // HT gender
            wr8(d, 0xC4, rd8(s, 0x93));          // CurrentHandler
            wr8(d, 0xC8, rd8(s, 0xA2));          // HT friendship
            copyBytes(d, 0xC9, s, 0xA4, 0x03);   // HT memory (intensity/memory/feeling)
            copyBytes(d, 0xCC, s, 0xA8, 0x02);   // HT text var
            copyBytes(d, 0xF8, s, 0xB0, 0x1A);   // OT name
            wr8(d, 0x112, rd8(s, 0xCA));         // OT friendship
            copyBytes(d, 0x119, s, 0xD1, 0x06);  // egg date (0xD1-0xD3) + met date (0xD4-0xD6) -> 0x119..0x11E
            copyBytes(d, 0x120, s, 0xD8, 0x04);  // egg location + met location
            wr8(d, 0x124, rd8(s, 0xDC));         // Ball (LGPE balls are <=26 -> valid in PK8)
            wr8(d, 0x125, rd8(s, 0xDD));         // MetLevel(bits0-6) + OTGender(bit7)
            wr8(d, 0x126, rd8(s, 0xDE));         // HyperTrain flags
            wr8(d, 0xDE, rd8(s, 0xDF));          // Version (PB7 0xDF -> PK8 0xDE)
            wr8(d, 0xE2, rd8(s, 0xE3));          // Language (PB7 0xE3 -> PK8 0xE2)
            return d;
        }

        // PK8 layout -> PB7 (Let's Go), returns a 260-byte record. Mirror of remapPB7toPK8. AVs + EVs are
        // left 0 (stat-training reset), CP + party stats are recomputed by the caller, PB7-only extras stay 0.
        std::vector<std::byte> remapPK8toPB7(const std::vector<std::byte>& s) {
            std::vector<std::byte> d(0x104, std::byte{0});   // PB7 = 260 bytes
            copyBytes(d, 0x00, s, 0x00, 0x08);   // EC + Sanity + Checksum
            copyBytes(d, 0x08, s, 0x08, 0x0C);   // Species / HeldItem / ID32 / EXP
            wr8(d, 0x14, rd8(s, 0x14));          // Ability (PK8 u16 low byte -> PB7 u8; LGPE ability ids < 256)
            wr8(d, 0x15, rd8(s, 0x16) & 0x07);   // AbilityNumber (PK8 0x16 -> PB7 0x15)
            copyBytes(d, 0x16, s, 0x18, 0x02);   // MarkingValue (PK8 0x18 -> PB7 0x16)
            copyBytes(d, 0x18, s, 0x1C, 0x04);   // PID
            wr8(d, 0x1C, rd8(s, 0x20));          // Nature
            { uint8_t g = rd8(s, 0x22); uint8_t fateful = g & 0x01, gender = (g >> 2) & 0x03, form = rd8(s, 0x24);
              wr8(d, 0x1D, fateful | (gender << 1) | (form << 3)); }  // PB7 0x1D: Fateful|Gender(bits1-2)|Form(bits3+)
            // EVs (0x1E-0x23) + AVs (0x24-0x29) deliberately left 0 -> entering LGPE resets stat training.
            wr8(d, 0x2B, rd8(s, 0x32));          // PokerusState
            copyBytes(d, 0x3A, s, 0x50, 0x02);   // Height + Weight scalars
            copyBytes(d, 0x3C, s, 0xE4, 0x04);   // FormArgument
            copyBytes(d, 0x40, s, 0x58, 0x1A);   // Nickname
            copyBytes(d, 0x5A, s, 0x72, 0x08);   // Moves
            copyBytes(d, 0x62, s, 0x7A, 0x04);   // Move PP
            copyBytes(d, 0x66, s, 0x7E, 0x04);   // Move PP-Ups
            copyBytes(d, 0x6A, s, 0x82, 0x08);   // Relearn
            copyBytes(d, 0x74, s, 0x8C, 0x04);   // IV32
            copyBytes(d, 0x78, s, 0xA8, 0x1A);   // HT name
            wr8(d, 0x92, rd8(s, 0xC2));          // HT gender
            wr8(d, 0x93, rd8(s, 0xC4));          // CurrentHandler
            wr8(d, 0xA2, rd8(s, 0xC8));          // HT friendship
            copyBytes(d, 0xA4, s, 0xC9, 0x03);   // HT memory
            copyBytes(d, 0xA8, s, 0xCC, 0x02);   // HT text var
            copyBytes(d, 0xB0, s, 0xF8, 0x1A);   // OT name
            wr8(d, 0xCA, rd8(s, 0x112));         // OT friendship
            copyBytes(d, 0xD1, s, 0x119, 0x06);  // egg + met dates
            copyBytes(d, 0xD8, s, 0x120, 0x04);  // egg + met locations
            wr8(d, 0xDC, rd8(s, 0x124));         // Ball
            wr8(d, 0xDD, rd8(s, 0x125));         // MetLevel + OTGender
            wr8(d, 0xDE, rd8(s, 0x126));         // HyperTrain flags
            wr8(d, 0xDF, rd8(s, 0xDE));          // Version
            wr8(d, 0xE3, rd8(s, 0xE2));          // Language
            // LGPE has no held-item mechanic (PKHeX likewise drops held items entering LGPE) -> drop it.
            // Illegal / nonexistent moves are cleared by the general moveset sanitizer in convert().
            wr8(d, 0x0A, 0); wr8(d, 0x0B, 0);
            // Absolute height/weight (0x2C/0xE4 floats): LGPE displays these; PK8/PK9 store only the
            // scalar and compute the absolute on the fly. Recompute from the carried scalars + the
            // species/form base size, per PKHeX PB7.GetHeightAbsolute / GetWeightAbsolute.
            {
                const uint16_t species = static_cast<uint16_t>(rd8(d, 0x08)) | (static_cast<uint16_t>(rd8(d, 0x09)) << 8);
                const uint8_t form = static_cast<uint8_t>(rd8(d, 0x1D) >> 3);   // PB7 packs form in 0x1D bits 3+
                const Pokemon::PersonalInfo& pi = Pokemon::getPersonalInfo(species, form);
                const float hr = (rd8(d, 0x3A) / 255.0f) * 0.79999995f + 0.6f;   // height ratio (+40% / -20%)
                const float wr = (rd8(d, 0x3B) / 255.0f) * 0.40000004f + 0.8f;   // weight ratio (+/- 20%)
                wrf32(d, 0x2C, hr * static_cast<float>(pi.height));              // HeightAbsolute
                wrf32(d, 0xE4, hr * wr * static_cast<float>(pi.weight));         // WeightAbsolute
            }
            return d;
        }

        // ---- FireRed/LeafGreen (PK3) <-> PK8 remap ---------------------------------------------
        // Gen 3 is the most divergent format: no EC (EC := PID), nature/gender/shiny all PID-derived,
        // ability = a slot BIT (IV32 bit31) resolved through the personal table, INTERNAL species ids,
        // Gen 3 item ids, Gen 3 text (not UTF-16), IVs packed with the ability bit at 31, and no relearn
        // moves / HT / memories / marks. We map identity + stats + moves + origin and drop the rest;
        // held item + moveset are remapped/sanitized to the true destination in convert().

        // PK3 (canonical decrypted, 80/100 B) -> PK8 layout (0x148 stored).
        std::vector<std::byte> remapPK3toPK8(const std::vector<std::byte>& s) {
            std::vector<std::byte> d(0x148, std::byte{0});
            const uint32_t pid      = rd32(s, 0x00);
            const uint16_t national = Pokemon::g3ToNational(rd16(s, 0x20));
            const uint32_t iv32     = rd32(s, 0x48);
            const uint16_t origins  = rd16(s, 0x46);
            const uint8_t  abilBit  = (iv32 >> 31) & 1;

            uint8_t form = 0;   // Gen 3 stores no form except Unown (PID-derived)
            if (national == 201) {
                const uint32_t v = ((pid & 0x03000000u) >> 18) | ((pid & 0x00030000u) >> 12)
                                 | ((pid & 0x00000300u) >> 6)  | (pid & 0x00000003u);
                form = static_cast<uint8_t>(v % 28);
            }
            const Pokemon::PersonalInfo& pi = Pokemon::getPersonalInfo(national, form);

            wr32(d, 0x00, pid);                                     // EC := PID
            wr16(d, 0x08, national);                                // Species (National)
            wr16(d, 0x0A, Names::itemG3ToModern(rd16(s, 0x22)));    // Held item (Gen 3 -> modern; 0 if none)
            copyBytes(d, 0x0C, s, 0x04, 4);                         // ID32 (TID16 + SID16)
            copyBytes(d, 0x10, s, 0x24, 4);                         // EXP
            wr16(d, 0x14, abilBit ? pi.ability2 : pi.ability1);     // Ability id (from the slot bit)
            wr8(d, 0x16, abilBit ? 2 : 1);                          // AbilityNumber (1=slot1 / 2=slot2)
            wr32(d, 0x1C, pid);                                     // PID
            wr8(d, 0x20, static_cast<uint8_t>(pid % 25));           // Nature
            wr8(d, 0x21, static_cast<uint8_t>(pid % 25));           // StatNature (no mints pre-Gen 8)

            uint8_t gender;
            const uint8_t gr = pi.genderRatio;
            if (gr == 255) gender = 2; else if (gr == 254) gender = 1; else if (gr == 0) gender = 0;
            else gender = ((pid & 0xFF) < gr) ? 1 : 0;
            const uint8_t fateful = (rd32(s, 0x4C) >> 31) & 1;      // Gen 3 ribbon bit31 = FatefulEncounter
            wr8(d, 0x22, static_cast<uint8_t>(fateful | (gender << 2)));
            wr8(d, 0x24, form);

            copyBytes(d, 0x26, s, 0x38, 6);                         // EVs (HP,ATK,DEF,SPE,SPA,SPD -- same order)
            copyBytes(d, 0x2C, s, 0x3E, 6);                         // Contest stats
            wr8(d, 0x32, rd8(s, 0x44));                             // Pokerus

            copyBytes(d, 0x72, s, 0x2C, 8);                         // Moves 1-4
            copyBytes(d, 0x7A, s, 0x34, 4);                         // Move PP
            { const uint8_t pu = rd8(s, 0x28); for (int i = 0; i < 4; ++i) wr8(d, 0x7E + i, (pu >> (i * 2)) & 3); }
            wr32(d, 0x8C, iv32 & 0x7FFFFFFFu);                      // IVs + isEgg(bit30); clear Gen 3 ability bit(31)

            g3NameToUtf16(d, 0x58, s, 0x08, 10, 26);                // Nickname (Gen 3 -> UTF-16)
            g3NameToUtf16(d, 0xF8, s, 0x14, 7, 26);                 // OT name
            wr8(d, 0x112, rd8(s, 0x29));                            // OT friendship (Gen 3 single friendship)
            wr8(d, 0x124, static_cast<uint8_t>((origins >> 11) & 0x0F));                             // Ball
            wr8(d, 0x125, static_cast<uint8_t>((origins & 0x7F) | (((origins >> 15) & 1) << 7)));    // MetLevel + OTgender
            wr16(d, 0x122, rd8(s, 0x45));                           // Met location (Gen 3 id -- carried)
            // Gen 3 records no met DATE, so leaving it 0 reads as "met 00/00/2000" in the destination.
            // Emulate Pokemon HOME: PKHeX's PK3.ConvertToPK4() stamps MetDate = EncounterDate.GetDateNDS()
            // (the transfer date). Do the same with today's date; it rides the PK8-hub met-date region
            // (0x11C-0x11E, year stored as year-2000) out to every destination format. Skip an egg -- an
            // unhatched egg has no met date until it hatches (isEgg is iv32 bit 30, carried above).
            if (!((iv32 >> 30) & 1)) {
                const std::time_t nowT = std::time(nullptr);
                if (const std::tm* lt = std::localtime(&nowT)) {
                    int year = lt->tm_year + 1900;
                    if (year < 2000) year = 2000;   // met-year byte is (year - 2000); never negative
                    wr8(d, 0x11C, static_cast<uint8_t>(year - 2000));      // Met_Year
                    wr8(d, 0x11D, static_cast<uint8_t>(lt->tm_mon + 1));   // Met_Month (1-12)
                    wr8(d, 0x11E, static_cast<uint8_t>(lt->tm_mday));      // Met_Day (1-31)
                }
            }
            wr8(d, 0xDE, static_cast<uint8_t>((origins >> 7) & 0x0F));   // Origin game (FR=4/LG=5, same PK8 enum)
            wr8(d, 0xE2, rd8(s, 0x12));                             // Language
            return d;
        }

        // PK8 layout -> PK3 (Gen 3). Returns a 100-byte PARTY record (box writes take the first 80 B).
        std::vector<std::byte> remapPK8toPK3(const std::vector<std::byte>& s) {
            std::vector<std::byte> d(0x64, std::byte{0});
            const uint32_t pid      = rd32(s, 0x1C);
            const uint16_t national = rd16(s, 0x08);
            const uint32_t pk8iv    = rd32(s, 0x8C);
            const bool     isEgg    = (pk8iv >> 30) & 1;

            // Gen 3 derives nature/gender/shiny/ability ALL from the PID, but the source stores nature
            // and gender EXPLICITLY -- copying the PID verbatim silently changes them (an LGPE Calm mon
            // read as Jolly in FR/LG). Like PKSM's downgrade (PKX::getRandomPID), reroll the PID so its
            // Gen-3-derived traits match the source's: nature, gender, shiny status and ability slot are
            // preserved. IVs are NOT touched (separate field at 0x48). This DOES change the PID (the mon's
            // identity) and yields a PID/IV pair that won't match a real Gen 3 RNG frame -- the UI warns
            // the user first. Falls back to the original PID if no match is found within the budget.
            uint32_t outPid = pid;
            {
                const uint8_t  natWant = rd8(s, 0x20) % 25;                                    // source nature
                const uint8_t  genWant = static_cast<uint8_t>((rd8(s, 0x22) >> 2) & 3);        // 0=M,1=F,2=genderless
                const uint8_t  gr      = getPersonalInfo(national, rd8(s, 0x24)).genderRatio;  // by species+form
                const uint8_t  abilBit = (rd8(s, 0x16) == 2) ? 1u : 0u;                        // AbilityNumber 2 -> slot 2
                const uint32_t tid32   = rd32(s, 0x0C);
                const uint16_t tsv     = static_cast<uint16_t>((tid32 & 0xFFFF) ^ (tid32 >> 16));
                const bool     shWant  = (static_cast<uint16_t>(((pid & 0xFFFF) ^ (pid >> 16)) ^ tsv) < 16);  // source threshold
                uint32_t cand = pid;
                for (int i = 0; i < 1000000; ++i) {
                    const uint8_t  g   = (gr == 255) ? 2 : (gr == 254) ? 1 : (gr == 0) ? 0 : (((cand & 0xFF) < gr) ? 1 : 0);
                    const uint16_t psv = static_cast<uint16_t>((cand & 0xFFFF) ^ (cand >> 16));
                    const bool     sh  = (static_cast<uint16_t>(psv ^ tsv) < 8);               // Gen 3 shiny threshold
                    if ((cand % 25) == natWant && g == genWant && sh == shWant && (cand & 1u) == abilBit) { outPid = cand; break; }
                    cand = cand * 0x41C64E6Du + 0x00006073u;                                   // Gen 3 LCG walk
                }
            }
            wr32(d, 0x00, outPid);                                 // PID rerolled to preserve nature/gender/shiny/ability
            copyBytes(d, 0x04, s, 0x0C, 4);                         // OTID32
            wr8(d, 0x12, rd8(s, 0xE2));                             // Language
            wr8(d, 0x13, static_cast<uint8_t>(0x02 | (isEgg ? 0x04 : 0)));   // Flags: HasSpecies (+ IsEgg)
            wr16(d, 0x20, Pokemon::nationalToG3(national));         // Species (INTERNAL Gen 3 id)
            wr16(d, 0x22, Names::itemModernToG3(rd16(s, 0x0A)));    // Held item (modern -> Gen 3; 0 if none)
            copyBytes(d, 0x24, s, 0x10, 4);                         // EXP

            // Gen 3 nickname = the uppercase species name (custom nicknames aren't carried down to Gen 3).
            { const char* nm = Pokemon::getSpeciesNameGen89(national);
              char up[16]; int k = 0;
              for (const char* p = nm; *p && k < 15; ++p) {
                  char c = *p; if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A'); up[k++] = c;
              }
              up[k] = 0; asciiToG3Name(d, 0x08, up, 10); }
            utf16ToG3Name(d, 0x14, s, 0xF8, 26, 7);                 // OT name (UTF-16 -> Gen 3)

            copyBytes(d, 0x2C, s, 0x72, 8);                         // Moves 1-4
            copyBytes(d, 0x34, s, 0x7A, 4);                         // Move PP
            { uint8_t pu = 0; for (int i = 0; i < 4; ++i) pu |= static_cast<uint8_t>((rd8(s, 0x7E + i) & 3) << (i * 2)); wr8(d, 0x28, pu); }
            copyBytes(d, 0x38, s, 0x26, 6);                         // EVs
            copyBytes(d, 0x3E, s, 0x2C, 6);                         // Contest stats
            wr8(d, 0x29, rd8(s, 0x112));                            // Friendship (from OT friendship)
            wr8(d, 0x44, rd8(s, 0x32));                             // Pokerus

            // Origins word: MetLevel(0-6) + Version(7-10) + Ball(11-14) + OTGender(15).
            const uint8_t metLevel = rd8(s, 0x125) & 0x7F;
            const uint8_t otGender = (rd8(s, 0x125) >> 7) & 1;
            uint8_t ball = rd8(s, 0x124);   if (ball == 0 || ball > 12) ball = 4;    // Gen 3 balls 1-12; default Poke Ball
            uint8_t version = rd8(s, 0xDE); if (version < 1 || version > 5) version = 4;  // clamp to a Gen 3 game (FR)
            wr16(d, 0x46, static_cast<uint16_t>((metLevel & 0x7F) | ((version & 0x0F) << 7)
                                              | ((ball & 0x0F) << 11) | ((otGender & 1) << 15)));
            wr8(d, 0x45, static_cast<uint8_t>(rd16(s, 0x122)));     // Met location (Gen 3 ids are u8)

            // IVs: keep bits 0-29 (IVs) + bit30 (isEgg); set bit31 = ability slot from the PK8 ability number.
            const uint8_t abilNum = rd8(s, 0x16);                  // 1=slot1, 2=slot2, 4=hidden (-> slot1 in Gen 3)
            wr32(d, 0x48, (pk8iv & 0x7FFFFFFFu) | ((abilNum == 2) ? 0x80000000u : 0u));
            return d;
        }
    }

    bool canConvert(const Pokemon::Pokemon& src, GameVersion destGroup, Result& result) {
        result = gate(src, destGroup);
        return result == Result::Ok || result == Result::SameGroup;
    }

    std::unique_ptr<Pokemon::Pokemon> convert(const Pokemon::Pokemon& src, GameVersion destGroup, Result& result) {
        result = gate(src, destGroup);
        if (result != Result::Ok) return nullptr;   // SameGroup / NotInDex / Blocked / Unsupported

        const GameVersion from = src.getGameGroup();

        // Copy the source's decrypted bytes; every field at a shared offset (identity, IVs/EVs, moves,
        // nickname/OT/HT, ribbons/marks, dates, ...) carries verbatim. Then fix only the regions that
        // differ: siblings null a couple of divergent fields; cross-gen runs the full PK8<->PK9 remap.
        std::vector<std::byte> buf(src.getData().data(), src.getData().data() + src.getDataSize());

        bool seededPartyTail = false;   // a hub remap built this mon a fresh Gen 8/9 battle-stat tail
        bool viaHub = false;            // not a sibling pairing -> re-laid-out through the PK8 hub

        // Sibling fast paths keep the shared layout in place (preserve everything, null a few divergent
        // fields). Every other pairing normalizes the source to the PK8 layout, then denormalizes to the
        // destination -- so PB7 / PA8 / PK9 all interoperate through one hub.
        if (isG8(from) && isG8(destGroup)) {         // SwSh <-> BDSP: drop the Technical-Record flags (BDSP has no TRs).
            for (size_t o = 0x127; o <= 0x134 && o < buf.size(); ++o) buf[o] = std::byte{0};
        } else if (isG9(from) && isG9(destGroup)) {  // S/V <-> Z-A: drop the fields that diverge between PK9 and PA9.
            for (size_t o = 0x94; o <= 0x9F && o < buf.size(); ++o) buf[o] = std::byte{0};  // Tera (PK9) / Plus-flags (PA9)
            for (size_t o = 0x4B; o <= 0x57 && o < buf.size(); ++o) buf[o] = std::byte{0};  // DLC-TM (PK9) / LevelBoost (PA9)
            if (0x23 < buf.size()) buf[0x23] = std::byte{0};                                // Alpha (PA9) / alignment (PK9)
        } else {
            viaHub = true;
            // 1. Normalize the source into the PK8 layout.
            if (isG9(from))                     transformG9toG8(buf);        // PK9 -> PK8
            else if (from == GameVersion::PLA)  buf = remapPA8toPK8(buf);    // PA8 -> PK8
            else if (from == GameVersion::GG)   buf = remapPB7toPK8(buf);    // PB7 -> PK8
            else if (from == GameVersion::FRLG) buf = remapPK3toPK8(buf);    // PK3 -> PK8

            // 1b. Those three hub remaps emit a STORED-size (0x148) buffer -- no battle-stat tail. Handing
            // that to a Gen 8/9 entity makes level()/statXXX() index PAST the allocation, so the mon shows a
            // random level and garbage stats that change on every placement (and lands in the destination
            // slot with whatever bytes were already there). Grow to the party size and seed the level from
            // EXP: recalculateStats() reads level() first and bails on 0, so the tail needs a real level
            // before it can compute against it. (PA8/PB7/PK3 destinations are unaffected -- their remaps
            // already emit a full party record and their level() derives from EXP rather than a tail byte.)
            if ((isG8(destGroup) || isG9(destGroup)) && buf.size() < PARTY_SIZE_G89) {
                buf.resize(PARTY_SIZE_G89, std::byte{0});
                const uint16_t sp = rd16(buf, 0x08);
                wr8(buf, 0x148, Pokemon::getLevelFromExp(rd32(buf, 0x10), Pokemon::getGrowthRate(sp)));
                seededPartyTail = true;
            }

            // 2. Denormalize the PK8 layout into the destination.
            if (isG9(destGroup))                     transformG8toG9(buf, src.speciesID(), src.form());  // PK8 -> PK9
            else if (destGroup == GameVersion::PLA)  buf = remapPK8toPA8(buf);                           // PK8 -> PA8
            else if (destGroup == GameVersion::GG)   buf = remapPK8toPB7(buf);                           // PK8 -> PB7
            else if (destGroup == GameVersion::FRLG) buf = remapPK8toPK3(buf);                           // PK8 -> PK3
        }

        // Re-key into the destination format, rebuild the entity, refresh its checksum.
        const uint32_t ec = Utils::readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(buf.data()));
        std::byte* enc = encryptFor(destGroup, std::span<const std::byte>(buf.data(), buf.size()), ec);
        if (!enc) { result = Result::Unsupported; return nullptr; }
        auto out = makePokemon(destGroup, std::span<const std::byte>(enc, buf.size()));
        delete[] enc;
        if (!out) { result = Result::Unsupported; return nullptr; }

        // Sanitize the moveset to what the destination game's species can legally learn: any move (or
        // relearn move) it can't -- illegal for the species, OR not present in that game at all -- is
        // cleared to empty. Required for LGPE, which turns a mon with an impossible move into a Bad Egg;
        // for the other games it just keeps the transfer to a legal moveset. Learnability is the per-game
        // pool from the learnset table. Applies to every cross-game conversion.
        {
            const uint16_t sp = out->speciesID();
            const uint8_t  fm = out->form();
            for (int i = 0; i < 4; ++i) {
                if (out->move(i) != 0 && !Pokemon::isLearnable(sp, fm, destGroup, out->move(i))) {
                    out->setMove(i, 0); out->setMovePP(i, 0); out->setMovePPUps(i, 0);
                }
                if (destGroup != GameVersion::FRLG && out->relearnMove(i) != 0
                    && !Pokemon::isLearnable(sp, fm, destGroup, out->relearnMove(i)))
                    out->setRelearnMove(i, 0);   // Gen 3 has no relearn moves (remap leaves them 0)
            }

            // Compact the surviving moves upward (#27). Clearing slot 2 of 4 otherwise leaves a HOLE,
            // and a gap mid-moveset is not a state the games produce: they pack moves from slot 1 and
            // treat the first empty slot as the end of the list. A mon that arrived as
            // [Tackle, --, Ember, --] could therefore read as knowing only Tackle. Each move carries its
            // own PP and PP-Ups with it, or the counts would end up attached to the wrong move.
            for (int dst = 0, src = 0; src < 4; ++src) {
                if (out->move(src) == 0) continue;
                if (dst != src) {
                    out->setMove(dst, out->move(src));
                    out->setMovePP(dst, out->movePP(src));
                    out->setMovePPUps(dst, out->movePPUps(src));
                    out->setMove(src, 0); out->setMovePP(src, 0); out->setMovePPUps(src, 0);
                }
                ++dst;
            }
        }

        // Sanitize the held item the same way: drop anything the destination can't actually hold.
        // Let's Go and Legends: Arceus have NO held-item mechanic at all (PKHeX Legal.HeldItems_GG and
        // HeldItems_LA are both empty), and every other game has its own item space where an id that
        // doesn't exist is meaningless. `out` is already in the destination's format, so its held-item id
        // is in that game's own space -- Gen 3 ids for FRLG, modern ids everywhere else.
        if (out->heldItem() != 0 && !Names::isHeldItemPresent(out->heldItem(), destGroup))
            out->setHeldItem(0);

        // Fill the destination's (un-checksummed) battle-stat tail whenever this conversion created a fresh
        // one: Let's Go and Gen 3 always build theirs from scratch (they store Level + stats + Combat Power
        // there, so a zeroed tail shows Level/CP 0), a Gen 8/9 mon arriving through a hub remap had its tail
        // seeded above, and a PA8 built by remapPK8toPA8 starts with a zeroed tail too. A same-gen *sibling*
        // pairing carries the source's tail verbatim and needs nothing.
        const bool freshTail = seededPartyTail || (viaHub && destGroup == GameVersion::PLA);
        if (destGroup == GameVersion::GG || destGroup == GameVersion::FRLG || freshTail)
            out->recalculateStats();

        // Heal to full if current HP reads 0. Current HP is NOT in the party stat tail -- it sits INSIDE the
        // stored (checksummed) region, so a hub remap that only maps the fields it knows about leaves it
        // zero and the converted mon arrives FAINTED in the destination game. Observed on hardware: a
        // FireRed Rattata in Shining Pearl with every stat correct but 0/15 HP. Offsets per PKHeX:
        // PK8/PB8/PK9/PA9 = 0x8A, PA8 = 0x92; PB7 and PK3 write their own in recalculateStats().
        // Guarded on "reads 0" so a sibling/transform path keeps the source's real (possibly damaged) HP.
        {
            size_t hpOfs = 0;
            if (isG8(destGroup) || isG9(destGroup)) hpOfs = 0x8A;
            else if (destGroup == GameVersion::PLA) hpOfs = 0x92;
            if (hpOfs != 0 && hpOfs + 1 < out->getDataSize()) {
                std::span<std::byte> d = out->getData();
                const uint16_t cur = static_cast<uint16_t>(static_cast<uint8_t>(d[hpOfs]))
                                   | (static_cast<uint16_t>(static_cast<uint8_t>(d[hpOfs + 1])) << 8);
                if (cur == 0) {
                    const uint16_t full = out->statHPMax();
                    d[hpOfs]     = static_cast<std::byte>(full & 0xFF);
                    d[hpOfs + 1] = static_cast<std::byte>((full >> 8) & 0xFF);
                }
            }
        }

        out->refreshChecksum();   // stored bytes changed -> recompute the entity checksum
        result = Result::Ok;
        return out;
    }

    const char* resultMessage(Result r) {
        switch (r) {
            case Result::Ok:          return "已转换";
            case Result::SameGroup:   return "同一游戏";
            case Result::NotInDex:    return "此游戏中无法获得该宝可梦";
            case Result::Blocked:     return "此游戏无法接收该宝可梦种类";
            case Result::Unsupported: return "暂不支持与此游戏之间的传送";
        }
        return "";
    }
}
