#ifndef ENCRYPTION_ENCRYPTION_H
#define ENCRYPTION_ENCRYPTION_H

#include <vector>
#include <span>

#include <sys/types.h>

#include "Save/Block.h"

using namespace Save;

namespace Encryption {

    // uint8_t, not u_int8_t: the latter is a BSD/glibc spelling that isn't standard C++ and doesn't
    // exist outside those libcs, which broke the host build of the save layer (#50).
    static const uint8_t StaticXorpad[] =
    {
        0xA0, 0x92, 0xD1, 0x06, 0x07, 0xDB, 0x32, 0xA1, 0xAE, 0x01, 0xF5, 0xC5, 0x1E, 0x84, 0x4F, 0xE3,
        0x53, 0xCA, 0x37, 0xF4, 0xA7, 0xB0, 0x4D, 0xA0, 0x18, 0xB7, 0xC2, 0x97, 0xDA, 0x5F, 0x53, 0x2B,
        0x75, 0xFA, 0x48, 0x16, 0xF8, 0xD4, 0x8A, 0x6F, 0x61, 0x05, 0xF4, 0xE2, 0xFD, 0x04, 0xB5, 0xA3,
        0x0F, 0xFC, 0x44, 0x92, 0xCB, 0x32, 0xE6, 0x1B, 0xB9, 0xB1, 0x2E, 0x01, 0xB0, 0x56, 0x53, 0x36,
        0xD2, 0xD1, 0x50, 0x3D, 0xDE, 0x5B, 0x2E, 0x0E, 0x52, 0xFD, 0xDF, 0x2F, 0x7B, 0xCA, 0x63, 0x50,
        0xA4, 0x67, 0x5D, 0x23, 0x17, 0xC0, 0x52, 0xE1, 0xA6, 0x30, 0x7C, 0x2B, 0xB6, 0x70, 0x36, 0x5B,
        0x2A, 0x27, 0x69, 0x33, 0xF5, 0x63, 0x7B, 0x36, 0x3F, 0x26, 0x9B, 0xA3, 0xED, 0x7A, 0x53, 0x00,
        0xA4, 0x48, 0xB3, 0x50, 0x9E, 0x14, 0xA0, 0x52, 0xDE, 0x7E, 0x10, 0x2B, 0x1B, 0x77, 0x6E, 0, // aligned to 0x80
    };

    // Hash salt bytes from
    static const uint8_t IntroHashBytes[] = {
        0x9E, 0xC9, 0x9C, 0xD7, 0x0E, 0xD3, 0x3C, 0x44, 0xFB, 0x93, 0x03, 0xDC, 0xEB, 0x39, 0xB4, 0x2A,
        0x19, 0x47, 0xE9, 0x63, 0x4B, 0xA2, 0x33, 0x44, 0x16, 0xBF, 0x82, 0xA2, 0xBA, 0x63, 0x55, 0xB6,
        0x3D, 0x9D, 0xF2, 0x4B, 0x5F, 0x7B, 0x6A, 0xB2, 0x62, 0x1D, 0xC2, 0x1B, 0x68, 0xE5, 0xC8, 0xB5,
        0x3A, 0x05, 0x90, 0x00, 0xE8, 0xA8, 0x10, 0x3D, 0xE2, 0xEC, 0xF0, 0x0C, 0xB2, 0xED, 0x4F, 0x6D
    };

    static const uint8_t OutroHashBytes[] = {
        0xD6, 0xC0, 0x1C, 0x59, 0x8B, 0xC8, 0xB8, 0xCB, 0x46, 0xE1, 0x53, 0xFC, 0x82, 0x8C, 0x75, 0x75,
        0x13, 0xE0, 0x45, 0xDF, 0x32, 0x69, 0x3C, 0x75, 0xF0, 0x59, 0xF8, 0xD9, 0xA2, 0x5F, 0xB2, 0x17,
        0xE0, 0x80, 0x52, 0xDB, 0xEA, 0x89, 0x73, 0x99, 0x75, 0x79, 0xAF, 0xCB, 0x2E, 0x80, 0x07, 0xE6,
        0xF1, 0x26, 0xE0, 0x03, 0x0A, 0xE6, 0x6F, 0xF6, 0x41, 0xBF, 0x7E, 0x59, 0xC2, 0xAE, 0x55, 0xFD
    };

    void cryptStaticXorpadBytes(std::vector<uint8_t> &data, size_t dataLength);
    std::vector<Save::Block> decrypt(uint8_t* data, size_t dataLength);
    std::vector<uint8_t> encrypt(const std::vector<Save::Block>& blocks);
    void computeHash(const uint8_t* data, size_t dataLength, uint8_t* hash);

    /**
     * Block position table for shuffling/unshuffling Pokemon data blocks.
     *
     * The shuffle value is derived from bits 13-17 of the personality value:
     *   shuffleValue = (personalityValue >> 13) & 31
     *
     * This table maps shuffle values to block orders. Each group of 4 values
     * represents the positions of blocks A, B, C, D for a given shuffle value.
     *
     * For example, shuffle value 0 = [0,1,2,3] (no shuffle)
     *              shuffle value 1 = [0,1,3,2] (blocks C and D swapped)
     *
     * The table includes duplicates (entries 24-31) to eliminate modulus operations.
     */
    static const std::uint8_t blockPosition[128] = {
        // Shuffle values 0-23
        0, 1, 2, 3,  // shuffleValue 0: ABCD (no shuffle)
        0, 1, 3, 2,  // shuffleValue 1: ABDC
        0, 2, 1, 3,  // shuffleValue 2: ACBD
        0, 3, 1, 2,  // shuffleValue 3: ACDB
        0, 2, 3, 1,  // shuffleValue 4: ADBC
        0, 3, 2, 1,  // shuffleValue 5: ADCB
        1, 0, 2, 3,  // shuffleValue 6: BACD
        1, 0, 3, 2,  // shuffleValue 7: BADC
        2, 0, 1, 3,  // shuffleValue 8: BCAD (continued...)
        3, 0, 1, 2,
        2, 0, 3, 1,
        3, 0, 2, 1,
        1, 2, 0, 3,
        1, 3, 0, 2,
        2, 1, 0, 3,
        3, 1, 0, 2,
        2, 3, 0, 1,
        3, 2, 0, 1,
        1, 2, 3, 0,
        1, 3, 2, 0,
        2, 1, 3, 0,
        3, 1, 2, 0,
        2, 3, 1, 0,
        3, 2, 1, 0,  // shuffleValue 23

        // Duplicates of 0-7 to eliminate modulus (shuffleValue 24-31)
        0, 1, 2, 3,
        0, 1, 3, 2,
        0, 2, 1, 3,
        0, 3, 1, 2,
        0, 2, 3, 1,
        0, 3, 2, 1,
        1, 0, 2, 3,
        1, 0, 3, 2,
    };

    /**
     * Inverse block position table for unshuffling.
     *
     * This table is used during decryption to restore blocks to their original order.
     * Given a shuffle value, it provides the inverse permutation.
     */
    static const std::uint8_t blockPositionInvert[32] {
        0, 1, 2, 4, 3, 5, 6, 7, 12, 18, 13, 19, 8, 10, 14, 20, 16, 22, 9, 11, 15, 21, 17, 23,
        0, 1, 2, 4, 3, 5, 6, 7, // Duplicates of 0-7 to eliminate modulus
    };


    /**
     * XOR encryption/decryption using Linear Congruential Generator (LCG).
     *
     * The LCG formula is: seed = (multiplier * seed) + increment
     * Multiplier: 0x41C64E6D
     * Increment:  0x00006073
     *
     * This is the same LCG used in many Pokemon games for random number generation.
     *
     * Process:
     * 1. Advance the LCG state
     * 2. Extract upper 16 bits as XOR mask
     * 3. XOR the mask with current 16-bit data chunk
     * 4. Repeat for all 16-bit chunks in the data
     */
    void cryptArray(std::span<std::byte> data, uint32_t seed);


    /**
     * Decrypts/encrypts a Pokemon's data blocks and party stats.
     *
     * Pokemon data structure:
     * - Bytes 0-7:   Header (Encryption Constant + Checksum) - NOT encrypted
     * - Bytes 8-327: Four 80-byte blocks (Growth, Attacks, EVs, Misc) - ENCRYPTED
     * - Bytes 328+:  Party stats (if present) - ENCRYPTED
     *
     * This function decrypts both the data blocks and party stats sections.
     */
    void cryptPokemon(std::span<std::byte> data, uint32_t partyValue, size_t blockSize, size_t blockCount);
}

#endif