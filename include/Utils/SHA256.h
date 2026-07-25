#ifndef UTILS_SHA256_H
#define UTILS_SHA256_H

#include <cstdint>
#include <cstring>

namespace Utils {
    constexpr size_t PKSE_SHA256_BLOCK_SIZE = 64;
    constexpr size_t PKSE_SHA256_HASH_SIZE = 32;

    class SHA256 {
    public:
        SHA256();
        void update(const uint8_t* data, size_t length);
        void finalize(uint8_t* hash);

    private:
        void transform();
        void pad();

        uint32_t state[8];
        uint64_t bitCount;
        uint8_t buffer[PKSE_SHA256_BLOCK_SIZE];
        size_t bufferSize;
    };
}

#endif
