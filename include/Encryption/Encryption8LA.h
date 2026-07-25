/**
 * Encryption8LA.h - Pokemon Legends: Arceus (PA8) Encryption/Decryption Utilities
 *
 * Encryption/decryption for Legends: Arceus (PA8) Pokemon data. Same shuffle + LCG-XOR
 * algorithm as Sword/Shield, but PA8 blocks are 0x58 bytes (vs 0x50).
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

#ifndef ENCRYPTION_ENCRYPTION8_LA_H
#define ENCRYPTION_ENCRYPTION8_LA_H

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
    constexpr size_t BLOCK_COUNT8_LA = 4;

    /**
     * Size of each block in bytes (Legends: Arceus / PA8).
     * Each PA8 block is 88 bytes (0x58) — 8 more than SwSh/BDSP's 0x50, which shifts
     * every field after Block A relative to PK8/PK9.
     */
    constexpr size_t SIZE_BLOCK8_LA = 0x58;

    /**
     * Size of stored Pokemon data (PA8).
     * Includes 8-byte header + 4 blocks (0x58 each) = 360 bytes (0x168).
     */
    constexpr size_t SIZE_STORED8_LA = 8 + (BLOCK_COUNT8_LA * SIZE_BLOCK8_LA);

    /**
     * Size of party Pokemon data (PA8).
     * Stored data + 16 bytes of party stats = 376 bytes (0x178).
     */
    constexpr size_t SIZE_PARTY8_LA = SIZE_STORED8_LA + 0x10;

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
    void shuffleArray8LA(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue);

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
     * @param encryptedData The encrypted Pokemon data (SIZE_PARTY8_LA bytes)
     * @return Pointer to decrypted data (caller must delete[])
     */
    std::byte* decryptArray8LA(std::span<const std::byte> encryptedData);

    /**
     * Encrypts a Generation 8 Pokemon byte array.
     *
     * This is the reverse of decryptArray8LA, used when saving Pokemon data
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
    std::byte* encryptArray8LA(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif
