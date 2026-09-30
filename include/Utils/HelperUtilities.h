#ifndef UTILS_HELPER_UTILITIES_H
#define UTILS_HELPER_UTILITIES_H

#include <cstdint>
#include <string>
#include "Utils/NXTypes.h"   // u64; <switch.h> on console, plain typedefs off it
#include <cstdint>

#include "Utils/StringHelpers.h"
#include "Utils/Logger.h"

namespace Utils {
    uint16_t readUInt16LittleEndian(const uint8_t* pointer);

    int32_t readInt32LittleEndian(const uint8_t* pointer);

    uint32_t readUInt32LittleEndian(const uint8_t* pointer);

    uint64_t readUInt64LittleEndian(const uint8_t* pointer);

    void writeUInt16LittleEndian(uint8_t* pointer, uint16_t value);

    void writeUInt32LittleEndian(uint8_t* pointer, uint32_t value);

    void writeUInt64LittleEndian(uint8_t* pointer, uint64_t value);

    /// Returns a pseudo-random 32-bit value. Backed by a process-lifetime std::mt19937 seeded
    /// once from the libnx system tick. Used by the Pokemon creator for a new pokemon's PID /
    /// EncryptionConstant (a unique-per-call value is sufficient there).
    uint32_t rand32() noexcept;

    /// Gets the version string for a title from its NACP data (e.g., "1.0.1", "1.3.2")
    std::string getTitleVersion(u64 titleId);
}

#endif