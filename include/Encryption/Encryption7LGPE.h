/**
 * Encryption7LGPE.h - Generation 7 Let's Go Pikachu/Eevee Pokemon Encryption/Decryption Utilities
 *
 * This file contains encryption and decryption utilities for Generation 7 Pokemon data
 * (Pokemon Let's Go Pikachu/Eevee - Pokemon7LGPE format).
 *
 * Pokemon data in Gen 7 Let's Go is stored in an encrypted format using:
 * 1. A Linear Congruential Generator (LCG) for XOR encryption
 * 2. Block shuffling based on personality value
 *
 * Encryption Process:
 * 1. Four data blocks (Growth, Attacks, EVs, Misc) are shuffled based on shuffle value
 * 2. Blocks are encrypted using XOR with an LCG-generated keystream
 * 3. Encryption constant (EC) is used as the seed
 *
 * Decryption Process (reverse):
 * 1. XOR decryption using EC as seed
 * 2. Block unshuffling to restore original order
 */

#ifndef ENCRYPTION_ENCRYPTION7_LGPE_H
#define ENCRYPTION_ENCRYPTION7_LGPE_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Encryption/Encryption.h"

namespace Encryption {
    // ========================================
    // Generation 7 (Let's Go) Constants
    // ========================================

    /**
     * Number of data blocks in Pokemon structure.
     * Gen 7 uses 4 blocks: Growth, Attacks, EVs/Contest, and Misc.
     */
    constexpr size_t BLOCK_COUNT7_LGPE = 4;
    /**
     * Size of each block in bytes.
     * Each block contains 56 bytes (0x38) of Pokemon data.
     * This is SMALLER than Gen 8 blocks (80 bytes).
     */
    constexpr size_t SIZE_BLOCK7_LGPE = 0x38;

    /**
     * Size of stored Pokemon data.
     * Includes 8-byte header + 4 blocks = 232 bytes (0xE8).
     */
    constexpr size_t SIZE_STORED7_LGPE = 0xE8;

    /**
     * Size of party Pokemon data.
     * For PK7, both party and stored use the same size: 260 bytes (0x104).
     * This differs from Gen 8 which has separate sizes for party vs stored.
     */
    constexpr size_t SIZE_PARTY7_LGPE = 0x104;

    /**
     * Size specifically for Pokemon7LGPE format.
     * Always 260 bytes regardless of party or stored status.
     */
    constexpr size_t SIZE_POKEMON7_LGPE = 260;

    // ========================================
    // Encryption/Decryption Functions
    // ========================================

    /**
     * Encrypts or decrypts data using a Linear Congruential Generator (LCG).
     *
     * This is a symmetric operation - calling it twice with the same seed
     * restores the original data. The LCG formula used is:
     *   seed = (0x41C64E6D * seed) + 0x00006073
     *
     * The upper 16 bits of each generated value are used as XOR masks.
     *
     * @param data Span of bytes to encrypt/decrypt (modified in place)
     * @param seed Initial seed value (typically the Pokemon's Encryption Constant)
     */
    void cryptArray7LGPE(std::span<std::byte> data, uint32_t seed);

    /**
     * Decrypts a Pokemon's data blocks and party stats.
     *
     * This function decrypts:
     * 1. The 4 shuffled data blocks (Growth, Attacks, EVs, Misc)
     * 2. The party stats (if present, for party Pokemon only)
     *
     * The first 8 bytes (encryption constant + checksum) are not encrypted
     * and are skipped.
     *
     * @param data The encrypted Pokemon data (full SIZE_8PARTY or SIZE_8STORED)
     * @param personalityValue Used as encryption seed
     */
    void cryptPokemon7LGPE(std::span<std::byte> data, uint32_t personalityValue, size_t blockSize, size_t blockCount);

    /**
     * The LCRNG algorithm is identical across generations.
     */

    /**
     * Decrypts a Pokemon's data blocks using Gen 7 format.
     *
     * This function decrypts the 4 shuffled data blocks.
     * Gen 7 Let's Go does not have a separate party stats section
     * that needs decryption - all data is in the 260-byte structure.
     *
     * @param data The encrypted Pokemon data (modified in place)
     * @param personalityValue Used as encryption seed
     * @param blockSize Size of each block (SIZE_BLOCK7_LGPE = 56 bytes for Gen 7)
     */
    void cryptPokemon7LGPE(std::span<std::byte> data, uint32_t personalityValue, size_t blockSize);

    /**
     * Unshuffles the 4 data blocks based on the shuffle value (Gen 7 format).
     *
     * This function works identically to Gen 8 shuffling, but uses smaller blocks.
     * During encryption, Pokemon data blocks are shuffled in a deterministic order
     * based on the shuffle value derived from the personality value.
     *
     * @param data Source encrypted data (read-only)
     * @param result Destination for unshuffled data (must be same size as data)
     * @param shuffleValue Shuffle value (0-31), derived from (PV >> 13) & 31
     * @param blockSize Size of each block (SIZE_6BLOCK for Gen 7)
     */
    void shuffleArray7LGPE(std::span<const std::byte> data, std::span<std::byte> result, uint32_t shuffleValue, size_t blockSize);

    /**
     * Decrypts a Pokemon byte array.
     *
     * This is the main decryption function for Pokemon7LGPE Pokemon.
     *
     * Process:
     * 1. Extract Personality Value from first 4 bytes
     * 2. Calculate Shuffle Value from Personality Value
     * 3. Decrypt blocks using XOR cipher (CryptPKM7)
     * 4. Unshuffle blocks to restore original order
     *
     * @param encryptedData The encrypted Pokemon data (260 bytes for PK7)
     * @return Pointer to decrypted data (caller must delete[])
     */
    std::byte* decryptArray7LGPE(std::span<const std::byte> encryptedData);

    /**
     * Encrypts a Pokemon byte array.
     *
     * This is the reverse of decryptArray7LGPE, used when saving Pokemon7LGPE Pokemon data
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
    std::byte* encryptArray7LGPE(std::span<const std::byte> decryptedData, uint32_t personalityValue);
}

#endif
