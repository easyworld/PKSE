/**
 * Encryption8BDSP.h - Brilliant Diamond/Shining Pearl Pokemon Encryption/Decryption Utilities
 *
 * This file contains encryption and decryption utilities for Generation 8
 * Brilliant Diamond/Shining Pearl (BDSP) Pokemon data (PB8).
 *
 * The PB8 entity format is byte-for-byte identical to Sword/Shield's PK8
 * (same 0x148 stored / 0x158 party size, 0x50 block size, same shuffle + LCG
 * crypto). Per the generation-per-class convention this is a standalone per-game
 * copy of the Sword/Shield encryption code rather than a shared class.
 *
 * Pokemon data in Gen 8 is stored in an encrypted format using:
 * 1. A Linear Congruential Generator (LCG) for XOR encryption
 * 2. Block shuffling based on personality value
 *
 * Encryption Process:
 * 1. Four data blocks (Growth, Attacks, EVs, Misc) are shuffled based on shuffle value
 * 2. Blocks are encrypted using XOR with an LCG-generated keystream
 * 3. Encryption constant is used as the seed
 *
 * Decryption Process (reverse):
 * 1. XOR decryption using Encryption constant as seed
 * 2. Block unshuffling to restore original order
 */

#ifndef ENCRYPTION_ENCRYPTION8_BDSP_H
#define ENCRYPTION_ENCRYPTION8_BDSP_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption {
    // ========================================
    // Constants
    // ========================================

    /**
     * Number of data blocks in Pokemon structure.
     * Gen 8 uses 4 blocks: Growth, Attacks, EVs/Contest, and Misc.
     */
    constexpr size_t BLOCK_COUNT8_BDSP = 4;

    /**
     * Size of each block in bytes (Gen 8).
     * Each block contains 80 bytes (0x50) of Pokemon data.
     */
    constexpr size_t SIZE_BLOCK8_BDSP = 0x50;

    /**
     * Size of stored Pokemon data (Gen 8).
     * Includes 8-byte header + 4 blocks = 328 bytes (0x148).
     */
    constexpr size_t SIZE_STORED8_BDSP = 8 + (BLOCK_COUNT8_BDSP * SIZE_BLOCK8_BDSP);

    /**
     * Size of party Pokemon data (Gen 8).
     * Stored data + 16 bytes of party stats = 344 bytes (0x158).
     */
    constexpr size_t SIZE_PARTY8_BDSP = SIZE_STORED8_BDSP + 0x10;

    // ========================================
    // Encryption/Decryption Functions
    // ========================================

    /**
     * Unshuffles the 4 data blocks based on the shuffle value.
     *
     * During encryption, Pokemon data blocks are shuffled in a deterministic order
     * based on the shuffle value derived from the personality value. This function
     * reverses that shuffling to restore blocks to their natural order:
     *   Block A (Growth)    - Species, items, EVs, etc.
     *   Block B (Attacks)   - Moves, IVs, nickname
     *   Block C (EVs)       - Contest stats, ribbons
     *   Block D (Misc)      - OT info, encounter data
     *
     * @param data Source encrypted data (read-only)
     * @param result Destination for unshuffled data (must be same size as data)
     * @param shuffleValue (0-31), derived from (personalityValue >> 13) & 31
     */
    void shuffleArray8BDSP(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue);

    /**
     * Decrypts a Generation 8 Pokemon byte array.
     *
     * This is the main decryption function that combines XOR decryption and
     * block unshuffling to convert encrypted Pokemon data into readable format.
     *
     * Process:
     * 1. Extract Personality Value from first 4 bytes
     * 2. Calculate Shuffle Value from Personality Value
     * 3. Decrypt blocks using XOR cipher (CryptPokemon)
     * 4. Unshuffle blocks to restore original order
     *
     * @param encryptedData The encrypted Pokemon data (SIZE_PARTY8_BDSP bytes)
     * @return Pointer to decrypted data (caller must delete[])
     */
    std::byte* decryptArray8BDSP(std::span<const std::byte> encryptedData);

    /**
     * Encrypts a Generation 8 Pokemon byte array.
     *
     * This is the reverse of decryptArray8BDSP, used when saving Pokemon data
     * back to the save file.
     *
     * Process:
     * 1. Calculate Shuffle Value from Personality Value
     * 2. Shuffle blocks from normal order to encrypted positions
     * 3. Apply XOR cipher using Personality Value as seed
     *
     * @param decryptedData The decrypted Pokemon data (read-only)
     * @param personalityValue Used as encryption seed
     * @return Pointer to encrypted data (caller must delete[])
     */
    std::byte* encryptArray8BDSP(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif
