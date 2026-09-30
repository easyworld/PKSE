#ifndef ENCRYPTION_ENCRYPTION8_SWSH_H
#define ENCRYPTION_ENCRYPTION8_SWSH_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption {

    /// Growth, Attacks, EVs/Contest, Misc.
    constexpr size_t BLOCK_COUNT8_SWSH = 4;

    constexpr size_t SIZE_BLOCK8_SWSH = 0x50;

    constexpr size_t SIZE_STORED8_SWSH = 8 + (BLOCK_COUNT8_SWSH * SIZE_BLOCK8_SWSH);

    constexpr size_t SIZE_PARTY8_SWSH = SIZE_STORED8_SWSH + 0x10;

    void shuffleArray8SWSH(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue);

    std::byte* decryptArray8SWSH(std::span<const std::byte> encryptedData);

    std::byte* encryptArray8SWSH(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif
