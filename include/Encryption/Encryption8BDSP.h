/**
 * The PB8 record is byte-for-byte Sword/Shield's PK8: same 0x148 stored / 0x158 party size,
 * same 0x50 block, same shuffle and LCG.
 */

#ifndef ENCRYPTION_ENCRYPTION8_BDSP_H
#define ENCRYPTION_ENCRYPTION8_BDSP_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption {

    /// Growth, Attacks, EVs/Contest, Misc.
    constexpr size_t BLOCK_COUNT8_BDSP = 4;

    constexpr size_t SIZE_BLOCK8_BDSP = 0x50;

    constexpr size_t SIZE_STORED8_BDSP = 8 + (BLOCK_COUNT8_BDSP * SIZE_BLOCK8_BDSP);

    constexpr size_t SIZE_PARTY8_BDSP = SIZE_STORED8_BDSP + 0x10;

    void shuffleArray8BDSP(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue);

    std::byte* decryptArray8BDSP(std::span<const std::byte> encryptedData);

    std::byte* encryptArray8BDSP(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif
