/**
 * Encryption/decryption for Legends: Arceus (PA8) Pokemon data. Same shuffle + LCG-XOR
 * algorithm as Sword/Shield, but PA8 blocks are 0x58 bytes (vs 0x50).
 */

#ifndef ENCRYPTION_ENCRYPTION8_LA_H
#define ENCRYPTION_ENCRYPTION8_LA_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption {

    /// Growth, Attacks, EVs/Contest, Misc.
    constexpr size_t BLOCK_COUNT8_LA = 4;

    /// 88 bytes, 8 more than SwSh/BDSP's 0x50, which shifts every field after Block A.
    constexpr size_t SIZE_BLOCK8_LA = 0x58;

    constexpr size_t SIZE_STORED8_LA = 8 + (BLOCK_COUNT8_LA * SIZE_BLOCK8_LA);

    constexpr size_t SIZE_PARTY8_LA = SIZE_STORED8_LA + 0x10;

    void shuffleArray8LA(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue);

    std::byte* decryptArray8LA(std::span<const std::byte> encryptedData);

    std::byte* encryptArray8LA(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif
