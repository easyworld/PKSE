#ifndef ENCRYPTION_ENCRYPTION9_SV_H
#define ENCRYPTION_ENCRYPTION9_SV_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption {

    /// Growth, Attacks, EVs/Contest, Misc.
    constexpr size_t BLOCK_COUNT9_SV = 4;

    constexpr size_t SIZE_BLOCK9_SV = 0x50;

    constexpr size_t SIZE_STORED9_SV = 8 + (BLOCK_COUNT9_SV * SIZE_BLOCK9_SV);

    constexpr size_t SIZE_PARTY9_SV = SIZE_STORED9_SV + 0x10;

    constexpr size_t GAP_BOX_SLOT9_SV = 0x40;
    constexpr size_t GAP_PARTY_SLOT9_SV = 0x88;

    constexpr size_t PARTY_SLOT_SIZE9_SV = SIZE_PARTY9_SV + GAP_PARTY_SLOT9_SV;

    constexpr size_t BOX_SLOT_SIZE9_SV = SIZE_PARTY9_SV + GAP_BOX_SLOT9_SV;

    void shuffleArray9SV(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue);

    std::byte* decryptArray9SV(std::span<const std::byte> encryptedData);

    std::byte* encryptArray9SV(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif