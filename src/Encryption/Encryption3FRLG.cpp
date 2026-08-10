/**
 * Encryption3FRLG.cpp - Gen 3 (GBA / FRLG) PK3 decrypt/encrypt. See Encryption3FRLG.h.
 *
 * Tables + algorithm verified against PKHeX PokeCrypto.cs (BlockPosition / BlockPositionInvert,
 * CryptArray3). The 0x20-0x4F block is XORed per 32-bit word with (PID ^ OT_ID32)
 * and the four 12-byte substructures are shuffled by (PID % 24).
 */
#include "Encryption/Encryption3FRLG.h"

// The `if (n < 0x50) return` guards below prove every access sits in [0x00, 0x50) <= n, but GCC's
// loop-idiom pass rewrites the fixed 48-byte substructure copies into memcpy and then can't propagate
// the size guard to the new[] buffer -- it false-positives with bogus 2^63-byte ranges. Both warnings
// are the same false positive (accesses are guarded above); suppress them for this file.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wrestrict"
#endif

namespace Encryption {

    namespace {
        // BLOCK_POSITION[sv*4 + blk] = the stored slot that holds canonical block `blk` (0=G,1=A,2=E,3=M)
        // for shuffle value sv (= PID % 24). Un-shuffle: canonical[blk] = stored[BLOCK_POSITION[sv*4+blk]].
        constexpr uint8_t BLOCK_POSITION[24 * 4] = {
            0,1,2,3,  0,1,3,2,  0,2,1,3,  0,3,1,2,  0,2,3,1,  0,3,2,1,   // sv 0-5   (G first)
            1,0,2,3,  1,0,3,2,  2,0,1,3,  3,0,1,2,  2,0,3,1,  3,0,2,1,   // sv 6-11  (A first)
            1,2,0,3,  1,3,0,2,  2,1,0,3,  3,1,0,2,  2,3,0,1,  3,2,0,1,   // sv 12-17 (E first)
            1,2,3,0,  1,3,2,0,  2,1,3,0,  3,1,2,0,  2,3,1,0,  3,2,1,0,   // sv 18-23 (M first)
        };
        // Inverse index: encrypt shuffles using BLOCK_POSITION[BLOCK_POSITION_INVERT[sv]*4 + n].
        constexpr uint8_t BLOCK_POSITION_INVERT[24] = {
            0,1,2,4,3,5,6,7,12,18,13,19,8,10,14,20,16,22,9,11,15,21,17,23,
        };

        inline uint32_t rd32(const std::byte* p) {
            return static_cast<uint32_t>(static_cast<uint8_t>(p[0]))
                 | (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8)
                 | (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16)
                 | (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24);
        }
        inline void wr32(std::byte* p, uint32_t v) {
            p[0] = static_cast<std::byte>(v);         p[1] = static_cast<std::byte>(v >> 8);
            p[2] = static_cast<std::byte>(v >> 16);   p[3] = static_cast<std::byte>(v >> 24);
        }
    }

    uint16_t checksum3FRLG(std::span<const std::byte> d) {
        uint16_t chk = 0;
        for (size_t o = 0x20; o + 1 < 0x50 && o + 1 < d.size(); o += 2)
            chk += static_cast<uint16_t>(static_cast<uint8_t>(d[o])) |
                   (static_cast<uint16_t>(static_cast<uint8_t>(d[o + 1])) << 8);
        return chk;
    }

    std::byte* decryptArray3FRLG(std::span<const std::byte> raw) {
        const size_t n = raw.size();
        std::byte* out = new std::byte[n]();
        for (size_t i = 0; i < n; ++i) out[i] = raw[i];
        if (n < 0x50) return out;  // too small to hold the data block

        const uint32_t pid  = rd32(out + 0x00);
        const uint32_t seed = pid ^ rd32(out + 0x04);   // key = PID ^ OT_ID32
        const uint32_t sv   = pid % 24;

        for (size_t o = 0x20; o < 0x50; o += 4) wr32(out + o, rd32(out + o) ^ seed);  // XOR-decrypt

        std::byte tmp[48];
        for (int i = 0; i < 48; ++i) tmp[i] = out[0x20 + i];
        for (int blk = 0; blk < 4; ++blk) {                                           // un-shuffle -> canonical
            const int src = BLOCK_POSITION[sv * 4 + blk] * 12;
            for (int i = 0; i < 12; ++i) out[0x20 + blk * 12 + i] = tmp[src + i];
        }
        return out;
    }

    std::byte* encryptArray3FRLG(std::span<const std::byte> dec) {
        const size_t n = dec.size();
        std::byte* out = new std::byte[n]();
        for (size_t i = 0; i < n; ++i) out[i] = dec[i];
        if (n < 0x50) return out;

        const uint32_t pid  = rd32(out + 0x00);
        const uint32_t seed = pid ^ rd32(out + 0x04);
        const uint32_t inv  = BLOCK_POSITION_INVERT[pid % 24];

        std::byte tmp[48];
        for (int i = 0; i < 48; ++i) tmp[i] = out[0x20 + i];                          // canonical
        for (int blk = 0; blk < 4; ++blk) {                                           // shuffle -> stored order
            const int src = BLOCK_POSITION[inv * 4 + blk] * 12;
            for (int i = 0; i < 12; ++i) out[0x20 + blk * 12 + i] = tmp[src + i];
        }
        for (size_t o = 0x20; o < 0x50; o += 4) wr32(out + o, rd32(out + o) ^ seed);  // XOR-encrypt
        return out;
    }
}
