/**
 * THIS FORWARDS TO Encryption3FRLG RATHER THAN COPYING IT, and that is deliberate.
 *
 * Every other layer gets a clean copy per save-format group, because what differs between groups is
 * per-game DATA -- offsets, block sizes, pouch tables -- and a copy keeps each game readable in
 * isolation instead of hiding a mode flag. The PK3 crypt has none of that. It is one algorithm with
 * no per-game constants at all: XOR the 0x20-0x4F block with (PID ^ OT_ID32), then unshuffle the
 * four 12-byte substructures by (PID % 24). Encryption3FRLG.h says so itself -- "the layout is
 * identical for all five GBA games (R/S/E/FR/LG); this file is named for FRLG because that is the
 * game PKSE targets".
 *
 * Copying it would mean a second copy of the 24-entry shuffle table, which is the one thing in Gen 3
 * that a transcription error would corrupt silently -- a wrong permutation still decrypts to
 * something, and the checksum is computed over the result. One table, one implementation.
 *
 * The header exists anyway so RSE code names its own crypt, the way the convention expects, and so
 * that if Ruby/Sapphire ever turn out to need something FireRed does not, there is a file to put it
 * in rather than a flag to add.
 */
#ifndef ENCRYPTION_ENCRYPTION3_RSE_H
#define ENCRYPTION_ENCRYPTION3_RSE_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption3FRLG.h"

namespace Encryption
{
    constexpr size_t SIZE_STORED3_RSE = SIZE_STORED3_FRLG; // 80  bytes (box)
    constexpr size_t SIZE_PARTY3_RSE = SIZE_PARTY3_FRLG;   // 100 bytes (party: + 20 battle-stat bytes)
    constexpr size_t SIZE_HEADER3_RSE = SIZE_HEADER3_FRLG; // 32  bytes (unencrypted header)

    inline std::byte *decryptArray3RSE(std::span<const std::byte> raw) { return decryptArray3FRLG(raw); }

    inline std::byte *encryptArray3RSE(std::span<const std::byte> decrypted) { return encryptArray3FRLG(decrypted); }

    inline uint16_t checksum3RSE(std::span<const std::byte> decrypted) { return checksum3FRLG(decrypted); }
}

#endif // ENCRYPTION_ENCRYPTION3_RSE_H
