#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "Utils/HelperUtilities.h"
#include "Utils/StringHelpers.h"

namespace Utils {
    // Equivalent to C# LoadString
    /// Loads characters into the result buffer and returns the count of characters loaded.
    int loadString(const uint8_t* data, size_t data_size, char16_t* result, size_t result_capacity) {
        size_t i = 0;
        for (; i < data_size; i += 2) {
            uint16_t value = readUInt16LittleEndian(&data[i]);  // one 2-byte UTF-16 unit (matches i += 2); a UInt32 read here over-ran the buffer by 2 bytes on the final unit
            if (value == TerminatorNull) {
                break;
            }
            result[i / 2] = static_cast<char16_t>(value);
        }
        return static_cast<int>(i / 2);
    }

    // Equivalent to C# GetString
    /// Converts Generation 7-Beluga encoded data to a decoded string.
    std::u16string getString(const uint8_t* data, size_t data_size) {
        // Allocate a vector for the result (heap-based, as dynamic stack allocation is non-standard in C++17)
        // Size is data_size / 2 to account for byte pairs, plus some padding if needed; adjust based on expected max.
        std::vector<char16_t> result(data_size / 2 + 1);  // +1 for potential null terminator, though not used here
        
        int length = loadString(data, data_size, result.data(), result.size());
        
        // Construct the u16string from the loaded portion
        return std::u16string(result.data(), static_cast<size_t>(length));
    }

    /// Converts a UTF-16 string to UTF-8
    std::string utf16ToUtf8(const std::u16string& utf16str) {
        std::string utf8str;
        utf8str.reserve(utf16str.length() * 3);

        for (size_t i = 0; i < utf16str.length(); ++i) {
            char32_t codepoint = utf16str[i];

            // Handle surrogate pairs for characters outside the Basic Multilingual Plane
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF && i + 1 < utf16str.length()) {
                char16_t high = utf16str[i];
                char16_t low = utf16str[i + 1];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codepoint = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
                    ++i;
                }
            }

            // Convert to UTF-8
            if (codepoint <= 0x7F) {
                utf8str.push_back(static_cast<char>(codepoint));
            } else if (codepoint <= 0x7FF) {
                utf8str.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
                utf8str.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else if (codepoint <= 0xFFFF) {
                utf8str.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
                utf8str.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                utf8str.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else if (codepoint <= 0x10FFFF) {
                utf8str.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
                utf8str.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                utf8str.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                utf8str.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }

        return utf8str;
    }

    /// Converts a UTF-8 string to UTF-16 (inverse of utf16ToUtf8)
    std::u16string utf8ToUtf16(const std::string& utf8str) {
        std::u16string utf16str;
        utf16str.reserve(utf8str.length());

        const size_t n = utf8str.length();
        size_t i = 0;
        while (i < n) {
            const uint8_t b0 = static_cast<uint8_t>(utf8str[i]);
            char32_t codepoint;
            size_t extra;  // number of trailing continuation bytes

            if (b0 < 0x80) {                   // 1-byte (ASCII)
                codepoint = b0;
                extra = 0;
            } else if ((b0 & 0xE0) == 0xC0) {  // 2-byte sequence
                codepoint = b0 & 0x1F;
                extra = 1;
            } else if ((b0 & 0xF0) == 0xE0) {  // 3-byte sequence (rest of the BMP)
                codepoint = b0 & 0x0F;
                extra = 2;
            } else if ((b0 & 0xF8) == 0xF0) {  // 4-byte sequence (astral -> surrogate pair)
                codepoint = b0 & 0x07;
                extra = 3;
            } else {                           // invalid lead byte - skip it
                ++i;
                continue;
            }

            // Bail out on a truncated sequence at the end of the string.
            if (i + extra >= n) {
                break;
            }

            // Fold in the continuation bytes; a malformed one skips only this lead byte.
            bool valid = true;
            for (size_t k = 1; k <= extra; ++k) {
                const uint8_t bk = static_cast<uint8_t>(utf8str[i + k]);
                if ((bk & 0xC0) != 0x80) {
                    valid = false;
                    break;
                }
                codepoint = (codepoint << 6) | (bk & 0x3F);
            }
            if (!valid) {
                ++i;
                continue;
            }
            i += extra + 1;

            // Emit as UTF-16 (surrogate pair for code points outside the BMP).
            if (codepoint <= 0xFFFF) {
                utf16str.push_back(static_cast<char16_t>(codepoint));
            } else if (codepoint <= 0x10FFFF) {
                codepoint -= 0x10000;
                utf16str.push_back(static_cast<char16_t>(0xD800 + (codepoint >> 10)));
                utf16str.push_back(static_cast<char16_t>(0xDC00 + (codepoint & 0x3FF)));
            }
        }

        return utf16str;
    }

    /// Writes a UTF-16LE string into dest: up to maxChars code units, then
    /// zero-fills the remaining bytes (null terminator + padding). data_size is
    /// the full field width in bytes.
    void setString(uint8_t* dest, size_t data_size, const std::u16string& value, size_t maxChars) {
        if (dest == nullptr || data_size < 2) {
            return;
        }
        const size_t slots = data_size / 2;               // number of u16 slots available
        size_t writable = (slots > 0) ? slots - 1 : 0;    // reserve one slot for the terminator
        if (writable > maxChars) {
            writable = maxChars;
        }
        const size_t count = (value.size() < writable) ? value.size() : writable;

        size_t b = 0;
        for (size_t i = 0; i < count; ++i) {
            const uint16_t ch = static_cast<uint16_t>(value[i]);
            dest[b++] = static_cast<uint8_t>(ch & 0xFF);
            dest[b++] = static_cast<uint8_t>((ch >> 8) & 0xFF);
        }
        // Zero-fill the remainder (null terminator + any trailing bytes).
        for (; b < data_size; ++b) {
            dest[b] = 0;
        }
    }
}