#ifndef UTILS_STRING_HELPERS_H
#define UTILS_STRING_HELPERS_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace Utils {
    constexpr uint16_t TerminatorNull = 0u;

    // Equivalent to C# LoadString
    /// Loads characters into the result buffer and returns the count of characters loaded.
    int loadString(const uint8_t* data, size_t data_size, char16_t* result, size_t result_capacity);

    // Equivalent to C# GetString
    /// Converts Generation 7-Beluga encoded data to a decoded string.
    std::u16string getString(const uint8_t* data, size_t data_size);

    /// Converts a UTF-16 string to UTF-8
    std::string utf16ToUtf8(const std::u16string& utf16str);

    /// Converts a UTF-8 string to UTF-16. Covers the full BMP; astral code points are emitted as
    /// surrogate pairs. Malformed/truncated byte sequences are skipped. The inverse of utf16ToUtf8.
    std::u16string utf8ToUtf16(const std::string& utf8str);

    /// Writes a UTF-16LE string into dest (up to maxChars code units), then
    /// zero-fills the rest of the data_size bytes (null terminator + padding).
    /// data_size is the full field width in bytes (e.g. 26 for a 13-slot name).
    void setString(uint8_t* dest, size_t data_size, const std::u16string& value, size_t maxChars);
}

#endif