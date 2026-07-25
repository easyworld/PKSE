/**
 * Encryption9LZA.h - Generation 9 Legends: Z-A Pokemon Encryption/Decryption Utilities
 *
 * This file contains encryption and decryption utilities for Generation 9 Pokemon data
 * (Legends: Z-A — PK9).
 *
 * Pokemon data is stored in an encrypted format using:
 * 1. A Linear Congruential Generator (LCG) for XOR encryption
 * 2. Block shuffling based on personality value
 *
 * Encryption Process:
 * 1. Four data blocks (Growth, Attacks, EVs, Misc) are shuffled based on shuffle value
 * 2. Blocks are encrypted using XOR with an LCG-generated keystream
 * 3. Encryption Constant is used as the seed
 *
 * Decryption Process (reverse):
 * 1. XOR decryption using Encryption Constant as seed
 * 2. Block unshuffling to restore original order
 */

#ifndef ENCRYPTION_ENCRYPTION9_LZA_H
#define ENCRYPTION_ENCRYPTION9_LZA_H

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
     * Gen 9 uses 4 blocks: Growth, Attacks, EVs/Contest, and Misc.
     */
    constexpr size_t BLOCK_COUNT9_LZA = 4;

    // The size values are identical to Gen 8
    /**
     * Size of each block in bytes (Gen 9).
     * Each block contains 80 bytes (0x50) of Pokemon data.
     */
    constexpr size_t SIZE_BLOCK9_LZA = 0x50;

    /**
     * Size of stored Pokemon data (Gen 9).
     * Includes 8-byte header + 4 blocks = 328 bytes (0x148).
     */
    constexpr size_t SIZE_STORED9_LZA = 8 + (BLOCK_COUNT9_LZA * SIZE_BLOCK9_LZA);

    /**
     * Size of party Pokemon data (Gen 9).
     * Stored data + 16 bytes of party stats = 344 bytes (0x158).
     */
    constexpr size_t SIZE_PARTY9_LZA = SIZE_STORED9_LZA + 0x10;

    /**
     * Gap sizes for Pokemon storage in Gen 9.
     * These gaps exist between Pokemon data in party and box storage.
     */
    constexpr size_t GAP_BOX_SLOT9_LZA = 0x40;   // Gap after box Pokemon data
    constexpr size_t GAP_PARTY_SLOT9_LZA = 0x88; // Gap after party Pokemon data (0x40 + 0x48)

    /**
     * Size of party Pokemon slot including gaps (Gen 9).
     * SIZE_9PARTY (344 bytes) + GAP_PARTY_SLOT (0x88) = 480 bytes per slot
     */
    constexpr size_t PARTY_SLOT_SIZE9_LZA = SIZE_PARTY9_LZA + GAP_PARTY_SLOT9_LZA;

    /**
     * Size of box Pokemon slot including gaps (Gen 9).
     * SIZE_9PARTY (344 bytes) + GAP_BOX_SLOT (0x40) = 408 bytes per slot
     */
    constexpr size_t BOX_SLOT_SIZE9_LZA = SIZE_PARTY9_LZA + GAP_BOX_SLOT9_LZA;

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
    void shuffleArray9LZA(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue);

    /**
     * Decrypts a Generation 9 Pokemon byte array.
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
     * @param encryptedData The encrypted Pokemon data (SIZE_PARTY9_LZA bytes)
     * @return Pointer to decrypted data (caller must delete[])
     */
    std::byte* decryptArray9LZA(std::span<const std::byte> encryptedData);

    /**
     * Encrypts a Generation 9 Pokemon byte array.
     *
     * This is the reverse of decryptArray9LZA, used when saving Pokemon data
     * back to the save file.
     *
     * Process:
     * 1. Calculate Shuffle Value from Personality Value
     * 2. Shuffle blocks from normal order to encrypted positions
     * 3. Apply XOR cipher using PV as seed
     *
     * @param decryptedData The decrypted Pokemon data (read-only)
     * @param personalityValue Used as encryption seed
     * @return Pointer to encrypted data (caller must delete[])
     */
    std::byte* encryptArray9LZA(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif