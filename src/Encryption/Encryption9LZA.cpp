/**
 * Encryption9LZA.cpp - Generation 9 Legends: Z-A Pokemon Encryption/Decryption Implementation
 *
 * Implementation of encryption and decryption utilities.
 * See Encryption9LZA.h for detailed documentation.
 */

#include <cstring>

#include "Encryption/Encryption.h"
#include "Encryption/Encryption9LZA.h"
#include "Utils/HelperUtilities.h"

using namespace Utils;

namespace Encryption {
    // ========================================
    // Encryption/Decryption Functions
    // ========================================

    void shuffleArray9LZA(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue)
    {
        /**
         * Unshuffles the 4 data blocks based on the shuffle value.
         *
         * The shuffle value determines how the blocks were scrambled.
         * This function reads blocks in shuffled order and writes them back
         * in their natural order (A, B, C, D).
         *
         * Data Structure:
         * - Bytes 0-7:   Header (unchanged)
         * - Bytes 8-87:  Block A (Growth)
         * - Bytes 88-167: Block B (Attacks)
         * - Bytes 168-247: Block C (EVs/Contest)
         * - Bytes 248-327: Block D (Misc)
         * - Bytes 328+:  Party stats (unchanged, if present)
         */

        const uint32_t index = shuffleValue * BLOCK_COUNT9_LZA;
        constexpr uint32_t start = 8;

        // Copy first 8 bytes unchanged (encryption constant + checksum)
        std::memcpy(result.data(), data.data(), start);

        // Calculate end of shuffled region
        const auto end = start + (SIZE_BLOCK9_LZA * BLOCK_COUNT9_LZA);

        // Copy everything after shuffled blocks unchanged (party stats, if present)
        if (end < data.size())
        {
            const size_t remainingSize = data.size() - end;
            std::memcpy(result.data() + end, data.data() + end, remainingSize);
        }

        // Unshuffle the 4 blocks
        // Read from shuffled positions, write to natural positions
        for (uint32_t block = 0; block < BLOCK_COUNT9_LZA; block++)
        {
            // blockPosition[index + block] tells us which block is in position 'block'
            const int srcBlockIndex = blockPosition[index + block];
            const size_t srcOffset = start + (SIZE_BLOCK9_LZA * srcBlockIndex);
            const size_t destOffset = start + (SIZE_BLOCK9_LZA * block);

            // Copy one block from shuffled position to correct position
            std::memcpy(result.data() + destOffset, data.data() + srcOffset, SIZE_BLOCK9_LZA);
        }
    }

    std::byte* decryptArray9LZA(std::span<const std::byte> encryptedData)
    {
        /**
         * Main decryption function for Generation 9 Pokemon data.
         *
         * This function performs the complete decryption process:
         * 1. Extract encryption seed (Personality Value) from data
         * 2. Calculate shuffle value from Personality Value
         * 3. XOR decrypt the blocks
         * 4. Unshuffle blocks to restore natural order
         *
         * The result is Pokemon data in its natural, unencrypted block order,
         * ready for reading and modification.
         */

        // Extract Personality Value from first 4 bytes
        // Personality Value serves as the encryption seed
        const uint32_t personalityValue = readUInt32LittleEndian(reinterpret_cast<const uint8_t*>(encryptedData.data()));

        // Extract Shuffle Value from bits 13-17 of Personality Value
        // Shuffle Value determines how blocks were shuffled (0-31)
        const uint32_t shuffleValue = (personalityValue >> 13) & 31;

        // Create a mutable copy for XOR decryption
        std::byte* decryptedData = new std::byte[encryptedData.size()];
        std::memcpy(decryptedData, encryptedData.data(), encryptedData.size());
        std::span<std::byte> mutableSpan(decryptedData, encryptedData.size());

        // Decrypt the blocks using XOR cipher
        cryptPokemon(mutableSpan, personalityValue, SIZE_BLOCK9_LZA, BLOCK_COUNT9_LZA);

        // Unshuffle the blocks to their correct positions
        std::byte* unshuffledData = new std::byte[encryptedData.size()];
        std::span<std::byte> resultSpan(unshuffledData, encryptedData.size());
        shuffleArray9LZA(mutableSpan, resultSpan, shuffleValue);

        // Clean up intermediate buffer
        delete[] decryptedData;

        return unshuffledData;
    }

    std::byte* encryptArray9LZA(std::span<const std::byte> decryptedData, uint32_t personalityValue)
    {
        /**
         * Main encryption function for Generation 9 Pokemon data.
         *
         * This is the reverse of decryptArray9LZA. It takes unencrypted, unshuffled
         * Pokemon data and converts it back to the encrypted format used in save files.
         *
         * Process:
         * 1. Calculate shuffle value from Personality Value
         * 2. Shuffle blocks from natural order to encrypted positions
         * 3. Apply XOR cipher using Personality Value as seed
         */

        // Extract Shuffle Value from bits 13-17 of Personality Value
        const uint32_t shuffleValue = (personalityValue >> 13) & 31;

        // Create buffer for shuffled data
        std::byte* shuffledData = new std::byte[decryptedData.size()];
        std::span<std::byte> shuffledSpan(shuffledData, decryptedData.size());

        // Shuffle the blocks from normal positions to encrypted positions
        const uint32_t index = shuffleValue * BLOCK_COUNT9_LZA;
        constexpr uint32_t start = 8;

        // Copy first 8 bytes unchanged (encryption constant + checksum)
        std::memcpy(shuffledSpan.data(), decryptedData.data(), start);

        // Calculate end of shuffled region
        const auto end = start + (SIZE_BLOCK9_LZA * BLOCK_COUNT9_LZA);

        // Copy everything after shuffled blocks unchanged (party stats, if present)
        if (end < decryptedData.size())
        {
            const size_t remainingSize = decryptedData.size() - end;
            std::memcpy(shuffledSpan.data() + end, decryptedData.data() + end, remainingSize);
        }

        // Shuffle the 4 blocks (reverse of unshuffle)
        // Read from natural positions, write to shuffled positions
        for (uint32_t block = 0; block < BLOCK_COUNT9_LZA; block++)
        {
            // blockPosition[index + block] tells us where block 'block' should go
            const int destBlockIndex = blockPosition[index + block];
            const size_t srcOffset = start + (SIZE_BLOCK9_LZA * block);
            const size_t destOffset = start + (SIZE_BLOCK9_LZA * destBlockIndex);

            // Copy one block from decrypted position to shuffled position
            std::memcpy(shuffledSpan.data() + destOffset, decryptedData.data() + srcOffset, SIZE_BLOCK9_LZA);
        }

        // Apply XOR cipher (symmetric operation)
        cryptPokemon(shuffledSpan, personalityValue, SIZE_BLOCK9_LZA, BLOCK_COUNT9_LZA);

        return shuffledData;
    }
}
