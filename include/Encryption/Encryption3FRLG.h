/**
 * Encryption3FRLG.h - Generation 3 (GBA / FireRed-LeafGreen) Pokemon encryption.
 *
 * PK3 layout: a 32-byte unencrypted header (0x00-0x1F) + a 48-byte encrypted data block
 * (0x20-0x4F, four 12-byte substructures Growth/Attacks/EVs/Misc) + party-only stats (0x50-0x63).
 *
 * The data block is protected two ways (much simpler than Gen 6+ -- no LCG keystream):
 *   1. A plain reversible XOR of every 32-bit word in 0x20-0x4F with key = PID ^ OT_ID32.
 *   2. The four 12-byte substructures are shuffled into one of 24 orders keyed by (PID % 24).
 *
 * Decrypt = XOR-decrypt then un-shuffle into canonical G/A/E/M order.
 * Encrypt = shuffle out of canonical order then XOR-encrypt.
 * PID (0x00) and OT_ID32 (0x04) live in the unencrypted header, so both directions read them there.
 * The checksum (header 0x1C) is a 16-bit word-sum over the 48 canonical (decrypted) bytes.
 *
 * The layout is identical for all five GBA games (R/S/E/FR/LG); this file is named for FRLG because
 * that is the game PKSE targets. Offsets + tables: docs/ROADMAP_TO_V1.md App. A (PKHeX PK3.cs / PokeCrypto.cs).
 */
#ifndef ENCRYPTION_ENCRYPTION3_FRLG_H
#define ENCRYPTION_ENCRYPTION3_FRLG_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption {

    constexpr size_t SIZE_STORED3_FRLG = 0x50;   // 80  bytes (box)
    constexpr size_t SIZE_PARTY3_FRLG  = 0x64;   // 100 bytes (party: + 20 battle-stat bytes)
    constexpr size_t SIZE_HEADER3_FRLG = 0x20;   // 32  bytes (unencrypted header)
    constexpr size_t SIZE_BLOCK3_FRLG  = 0x0C;   // 12  bytes per substructure
    constexpr size_t BLOCK_COUNT3_FRLG = 4;      // Growth / Attacks / EVs / Misc

    /**
     * Decrypts a raw PK3 record: XOR-decrypts the 0x20-0x4F block, then un-shuffles the four
     * substructures into canonical G/A/E/M order. Returns a newly-allocated buffer of the same
     * size as `raw` (caller deletes[]). PID/OT_ID are read from the header (0x00/0x04).
     */
    std::byte* decryptArray3FRLG(std::span<const std::byte> raw);

    /**
     * Encrypts a canonical (decrypted, un-shuffled) PK3 buffer: shuffles the substructures back into
     * the PID-keyed order, then XOR-encrypts the 0x20-0x4F block. Returns a newly-allocated buffer
     * (caller deletes[]). Reads PID/OT_ID from the header. Refresh the checksum BEFORE encrypting.
     */
    std::byte* encryptArray3FRLG(std::span<const std::byte> decrypted);

    /** 16-bit word-sum checksum over the 48 canonical data bytes (0x20-0x4F). Stored at header 0x1C. */
    uint16_t checksum3FRLG(std::span<const std::byte> decrypted);
}

#endif  // ENCRYPTION_ENCRYPTION3_FRLG_H
