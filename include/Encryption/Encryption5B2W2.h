/**
 * Two things about the Gen 4/5 crypt are silent if you get them wrong -- a mis-seeded record
 * decrypts to garbage that no checksum will catch, because you write the checksum yourself.
 *
 *  1. THE BODY IS KEYED ON THE CHECKSUM, NOT THE PID. Gen 6 onward crypt both the body and the
 *     party tail with the same seed; Gen 4/5 crypt the body with the 16-bit checksum at 0x06 and
 *     only the party tail with the PID. (PKHeX PokeCrypto.Decrypt45.)
 *  2. SO THE CHECKSUM MUST BE REFRESHED BEFORE ENCRYPTING, not after. It is the key. Refreshing
 *     it afterwards encrypts with a stale key and the game reads a Bad Egg.
 *
 * The shuffle value is (PID >> 13) & 31 and there are four blocks of 0x20.
 */
#ifndef ENCRYPTION_ENCRYPTION5_B2W2_H
#define ENCRYPTION_ENCRYPTION5_B2W2_H

#include <cstddef>
#include <cstdint>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption
{
    /// Growth, Attacks, EVs/Contest, Misc.
    constexpr size_t BLOCK_COUNT5_B2W2 = 4;

    constexpr size_t SIZE_BLOCK5_B2W2 = 0x20;

    constexpr size_t SIZE_STORED5_B2W2 = 8 + (BLOCK_COUNT5_B2W2 * SIZE_BLOCK5_B2W2);

    /// The stored record plus this group's battle-stat tail.
    constexpr size_t SIZE_PARTY5_B2W2 = 0x0dc;

    /// True for either length this group's crypt accepts -- a group has exactly one party length.
    inline constexpr bool isSize5B2W2(size_t byteCount) noexcept
    {
        return byteCount == SIZE_STORED5_B2W2 || byteCount == SIZE_PARTY5_B2W2;
    }

    /// Returns a new[]'d buffer the caller owns.
    std::byte *decryptArray5B2W2(std::span<const std::byte> encryptedData);

    /// Encrypts one record. Reads its own seeds out of the DECRYPTED buffer -- the PID at 0x00 and
    /// the checksum at 0x06 -- so the checksum must already be correct. Returns a new[]'d buffer
    /// the caller owns.
    std::byte *encryptArray5B2W2(std::span<const std::byte> decryptedData);

    /// The canonical EMPTY slot, `size` bytes, owned by the library.
    ///
    /// An unused party or box slot in a real save does not hold raw zeros -- it holds
    /// encrypt(zeros), whose header is zero (the seed is 0, so the shuffle is the identity and
    /// the first masked word is 0) and whose body is the fixed seed-0 LCG pattern. Stamping zeros
    /// instead rewrites every empty slot in the file, which is what the unedited round trip
    /// catches. Same reasoning as SwSh's canonical encrypted blank, different constant.
    std::span<const std::byte> blankRecord5B2W2(size_t size);
}

#endif
